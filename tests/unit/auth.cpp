#include <iostream>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include "auth/auth.hpp"
#include "auth/crypto.hpp"
#include "utils/cast.hpp"
#include "utils/file.hpp"

using namespace auth;
namespace fs = std::experimental::filesystem;

DECLARE_bool(auth_password_permit_null);
DECLARE_string(auth_password_strength_regex);

class AuthWithStorage : public ::testing::Test {
 protected:
  virtual void SetUp() {
    utils::EnsureDir(test_folder_);
    FLAGS_auth_password_permit_null = true;
    FLAGS_auth_password_strength_regex = ".+";
  }

  virtual void TearDown() { fs::remove_all(test_folder_); }

  fs::path test_folder_{
      fs::temp_directory_path() /
      ("unit_auth_test_" + std::to_string(static_cast<int>(getpid())))};

  Auth auth{test_folder_};
};

TEST_F(AuthWithStorage, AddRole) {
  ASSERT_TRUE(auth.AddRole("admin"));
  ASSERT_TRUE(auth.AddRole("user"));
  ASSERT_FALSE(auth.AddRole("admin"));
}

TEST_F(AuthWithStorage, RemoveRole) {
  ASSERT_TRUE(auth.AddRole("admin"));
  ASSERT_TRUE(auth.RemoveRole("admin"));
  ASSERT_FALSE(auth.RemoveRole("user"));
  ASSERT_FALSE(auth.RemoveRole("user"));
}

TEST_F(AuthWithStorage, AddUser) {
  ASSERT_FALSE(auth.HasUsers());
  ASSERT_TRUE(auth.AddUser("test"));
  ASSERT_TRUE(auth.HasUsers());
  ASSERT_TRUE(auth.AddUser("test2"));
  ASSERT_FALSE(auth.AddUser("test"));
}

TEST_F(AuthWithStorage, RemoveUser) {
  ASSERT_FALSE(auth.HasUsers());
  ASSERT_TRUE(auth.AddUser("test"));
  ASSERT_TRUE(auth.HasUsers());
  ASSERT_TRUE(auth.RemoveUser("test"));
  ASSERT_FALSE(auth.HasUsers());
  ASSERT_FALSE(auth.RemoveUser("test2"));
  ASSERT_FALSE(auth.RemoveUser("test"));
  ASSERT_FALSE(auth.HasUsers());
}

TEST_F(AuthWithStorage, Authenticate) {
  ASSERT_FALSE(auth.HasUsers());

  auto user = auth.AddUser("test");
  ASSERT_NE(user, std::experimental::nullopt);
  ASSERT_TRUE(auth.HasUsers());

  ASSERT_TRUE(auth.Authenticate("test", "123"));

  user->UpdatePassword("123");
  auth.SaveUser(*user);

  ASSERT_NE(auth.Authenticate("test", "123"), std::experimental::nullopt);

  ASSERT_EQ(auth.Authenticate("test", "456"), std::experimental::nullopt);
  ASSERT_NE(auth.Authenticate("test", "123"), std::experimental::nullopt);

  user->UpdatePassword();
  auth.SaveUser(*user);

  ASSERT_NE(auth.Authenticate("test", "123"), std::experimental::nullopt);
  ASSERT_NE(auth.Authenticate("test", "456"), std::experimental::nullopt);

  ASSERT_EQ(auth.Authenticate("nonexistant", "123"),
            std::experimental::nullopt);
}

TEST_F(AuthWithStorage, UserRolePermissions) {
  ASSERT_FALSE(auth.HasUsers());
  ASSERT_TRUE(auth.AddUser("test"));
  ASSERT_TRUE(auth.HasUsers());

  auto user = auth.GetUser("test");
  ASSERT_NE(user, std::experimental::nullopt);

  // Test initial user permissions.
  ASSERT_EQ(user->permissions().Has(Permission::MATCH),
            PermissionLevel::NEUTRAL);
  ASSERT_EQ(user->permissions().Has(Permission::CREATE),
            PermissionLevel::NEUTRAL);
  ASSERT_EQ(user->permissions().Has(Permission::MERGE),
            PermissionLevel::NEUTRAL);
  ASSERT_EQ(user->permissions().Has(Permission::DELETE),
            PermissionLevel::NEUTRAL);
  ASSERT_EQ(user->permissions(), user->GetPermissions());

  // Change one user permission.
  user->permissions().Grant(Permission::MATCH);

  // Check permissions.
  ASSERT_EQ(user->permissions().Has(Permission::MATCH), PermissionLevel::GRANT);
  ASSERT_EQ(user->permissions().Has(Permission::CREATE),
            PermissionLevel::NEUTRAL);
  ASSERT_EQ(user->permissions().Has(Permission::MERGE),
            PermissionLevel::NEUTRAL);
  ASSERT_EQ(user->permissions().Has(Permission::DELETE),
            PermissionLevel::NEUTRAL);
  ASSERT_EQ(user->permissions(), user->GetPermissions());

  // Create role.
  ASSERT_TRUE(auth.AddRole("admin"));
  auto role = auth.GetRole("admin");
  ASSERT_NE(role, std::experimental::nullopt);

  // Assign permissions to role and role to user.
  role->permissions().Grant(Permission::DELETE);
  user->SetRole(*role);

  // Check permissions.
  {
    auto permissions = user->GetPermissions();
    ASSERT_EQ(permissions.Has(Permission::MATCH), PermissionLevel::GRANT);
    ASSERT_EQ(permissions.Has(Permission::DELETE), PermissionLevel::GRANT);
    ASSERT_EQ(permissions.Has(Permission::CREATE), PermissionLevel::NEUTRAL);
    ASSERT_EQ(permissions.Has(Permission::MERGE), PermissionLevel::NEUTRAL);
  }

  // Add explicit deny to role.
  role->permissions().Deny(Permission::MATCH);
  user->SetRole(*role);

  // Check permissions.
  {
    auto permissions = user->GetPermissions();
    ASSERT_EQ(permissions.Has(Permission::MATCH), PermissionLevel::DENY);
    ASSERT_EQ(permissions.Has(Permission::DELETE), PermissionLevel::GRANT);
    ASSERT_EQ(permissions.Has(Permission::CREATE), PermissionLevel::NEUTRAL);
    ASSERT_EQ(permissions.Has(Permission::MERGE), PermissionLevel::NEUTRAL);
  }
}

TEST_F(AuthWithStorage, RoleManipulations) {
  {
    auto user1 = auth.AddUser("user1");
    ASSERT_TRUE(user1);
    auto role1 = auth.AddRole("role1");
    ASSERT_TRUE(role1);
    user1->SetRole(*role1);
    auth.SaveUser(*user1);

    auto user2 = auth.AddUser("user2");
    ASSERT_TRUE(user2);
    auto role2 = auth.AddRole("role2");
    ASSERT_TRUE(role2);
    user2->SetRole(*role2);
    auth.SaveUser(*user2);
  }

  {
    auto user1 = auth.GetUser("user1");
    ASSERT_TRUE(user1);
    auto role1 = user1->role();
    ASSERT_TRUE(role1);
    ASSERT_EQ(role1->rolename(), "role1");

    auto user2 = auth.GetUser("user2");
    ASSERT_TRUE(user2);
    auto role2 = user2->role();
    ASSERT_TRUE(role2);
    ASSERT_EQ(role2->rolename(), "role2");
  }

  ASSERT_TRUE(auth.RemoveRole("role1"));

  {
    auto user1 = auth.GetUser("user1");
    ASSERT_TRUE(user1);
    auto role = user1->role();
    ASSERT_FALSE(role);

    auto user2 = auth.GetUser("user2");
    ASSERT_TRUE(user2);
    auto role2 = user2->role();
    ASSERT_TRUE(role2);
    ASSERT_EQ(role2->rolename(), "role2");
  }

  {
    auto role1 = auth.AddRole("role1");
    ASSERT_TRUE(role1);
  }

  {
    auto user1 = auth.GetUser("user1");
    ASSERT_TRUE(user1);
    auto role1 = user1->role();
    ASSERT_FALSE(role1);

    auto user2 = auth.GetUser("user2");
    ASSERT_TRUE(user2);
    auto role2 = user2->role();
    ASSERT_TRUE(role2);
    ASSERT_EQ(role2->rolename(), "role2");
  }

  {
    auto users = auth.AllUsers();
    std::sort(users.begin(), users.end(), [](const User &a, const User &b) {
      return a.username() < b.username();
    });
    ASSERT_EQ(users.size(), 2);
    ASSERT_EQ(users[0].username(), "user1");
    ASSERT_EQ(users[1].username(), "user2");
  }

  {
    auto roles = auth.AllRoles();
    std::sort(roles.begin(), roles.end(), [](const Role &a, const Role &b) {
      return a.rolename() < b.rolename();
    });
    ASSERT_EQ(roles.size(), 2);
    ASSERT_EQ(roles[0].rolename(), "role1");
    ASSERT_EQ(roles[1].rolename(), "role2");
  }

  {
    auto users = auth.AllUsersForRole("role2");
    ASSERT_EQ(users.size(), 1);
    ASSERT_EQ(users[0].username(), "user2");
  }
}

TEST_F(AuthWithStorage, UserRoleLinkUnlink) {
  {
    auto user = auth.AddUser("user");
    ASSERT_TRUE(user);
    auto role = auth.AddRole("role");
    ASSERT_TRUE(role);
    user->SetRole(*role);
    auth.SaveUser(*user);
  }

  {
    auto user = auth.GetUser("user");
    ASSERT_TRUE(user);
    auto role = user->role();
    ASSERT_TRUE(role);
    ASSERT_EQ(role->rolename(), "role");
  }

  {
    auto user = auth.GetUser("user");
    ASSERT_TRUE(user);
    user->ClearRole();
    auth.SaveUser(*user);
  }

  {
    auto user = auth.GetUser("user");
    ASSERT_TRUE(user);
    ASSERT_FALSE(user->role());
  }
}

TEST_F(AuthWithStorage, UserPasswordCreation) {
  {
    auto user = auth.AddUser("test");
    ASSERT_TRUE(user);
    ASSERT_TRUE(auth.Authenticate("test", "123"));
    ASSERT_TRUE(auth.Authenticate("test", "456"));
    ASSERT_TRUE(auth.RemoveUser(user->username()));
  }

  {
    auto user = auth.AddUser("test", "123");
    ASSERT_TRUE(user);
    ASSERT_TRUE(auth.Authenticate("test", "123"));
    ASSERT_FALSE(auth.Authenticate("test", "456"));
    ASSERT_TRUE(auth.RemoveUser(user->username()));
  }
}

TEST_F(AuthWithStorage, PasswordStrength) {
  const std::string kWeakRegex = ".+";
  // https://stackoverflow.com/questions/5142103/regex-to-validate-password-strength
  const std::string kStrongRegex =
      "^(?=.*[A-Z].*[A-Z])(?=.*[!@#$&*])(?=.*[0-9].*[0-9])(?=.*[a-z].*[a-z].*["
      "a-z]).{8,}$";

  const std::string kWeakPassword = "weak";
  const std::string kAlmostStrongPassword =
      "ThisPasswordMeetsAllButOneCriterion1234";
  const std::string kStrongPassword = "ThisIsAVeryStrongPassword123$";

  auto user = auth.AddUser("user");
  ASSERT_TRUE(user);

  FLAGS_auth_password_permit_null = true;
  FLAGS_auth_password_strength_regex = kWeakRegex;
  ASSERT_NO_THROW(user->UpdatePassword());
  ASSERT_NO_THROW(user->UpdatePassword(kWeakPassword));
  ASSERT_NO_THROW(user->UpdatePassword(kAlmostStrongPassword));
  ASSERT_NO_THROW(user->UpdatePassword(kStrongPassword));

  FLAGS_auth_password_permit_null = false;
  FLAGS_auth_password_strength_regex = kWeakRegex;
  ASSERT_THROW(user->UpdatePassword(), AuthException);
  ASSERT_NO_THROW(user->UpdatePassword(kWeakPassword));
  ASSERT_NO_THROW(user->UpdatePassword(kAlmostStrongPassword));
  ASSERT_NO_THROW(user->UpdatePassword(kStrongPassword));

  FLAGS_auth_password_permit_null = true;
  FLAGS_auth_password_strength_regex = kStrongRegex;
  ASSERT_NO_THROW(user->UpdatePassword());
  ASSERT_THROW(user->UpdatePassword(kWeakPassword), AuthException);
  ASSERT_THROW(user->UpdatePassword(kAlmostStrongPassword), AuthException);
  ASSERT_NO_THROW(user->UpdatePassword(kStrongPassword));

  FLAGS_auth_password_permit_null = false;
  FLAGS_auth_password_strength_regex = kStrongRegex;
  ASSERT_THROW(user->UpdatePassword(), AuthException);
  ASSERT_THROW(user->UpdatePassword(kWeakPassword), AuthException);
  ASSERT_THROW(user->UpdatePassword(kAlmostStrongPassword), AuthException);
  ASSERT_NO_THROW(user->UpdatePassword(kStrongPassword));
}

TEST(AuthWithoutStorage, Permissions) {
  Permissions permissions;
  ASSERT_EQ(permissions.grants(), 0);
  ASSERT_EQ(permissions.denies(), 0);

  permissions.Grant(Permission::MATCH);
  ASSERT_EQ(permissions.Has(Permission::MATCH), PermissionLevel::GRANT);
  ASSERT_EQ(permissions.grants(), utils::UnderlyingCast(Permission::MATCH));
  ASSERT_EQ(permissions.denies(), 0);

  permissions.Revoke(Permission::MATCH);
  ASSERT_EQ(permissions.Has(Permission::MATCH), PermissionLevel::NEUTRAL);
  ASSERT_EQ(permissions.grants(), 0);
  ASSERT_EQ(permissions.denies(), 0);

  permissions.Deny(Permission::MATCH);
  ASSERT_EQ(permissions.Has(Permission::MATCH), PermissionLevel::DENY);
  ASSERT_EQ(permissions.denies(), utils::UnderlyingCast(Permission::MATCH));
  ASSERT_EQ(permissions.grants(), 0);

  permissions.Grant(Permission::MATCH);
  ASSERT_EQ(permissions.Has(Permission::MATCH), PermissionLevel::GRANT);
  ASSERT_EQ(permissions.grants(), utils::UnderlyingCast(Permission::MATCH));
  ASSERT_EQ(permissions.denies(), 0);

  permissions.Deny(Permission::CREATE);
  ASSERT_EQ(permissions.Has(Permission::MATCH), PermissionLevel::GRANT);
  ASSERT_EQ(permissions.Has(Permission::CREATE), PermissionLevel::DENY);
  ASSERT_EQ(permissions.Has(Permission::MERGE), PermissionLevel::NEUTRAL);
  ASSERT_EQ(permissions.grants(), utils::UnderlyingCast(Permission::MATCH));
  ASSERT_EQ(permissions.denies(), utils::UnderlyingCast(Permission::CREATE));

  permissions.Grant(Permission::DELETE);
  ASSERT_EQ(permissions.Has(Permission::MATCH), PermissionLevel::GRANT);
  ASSERT_EQ(permissions.Has(Permission::CREATE), PermissionLevel::DENY);
  ASSERT_EQ(permissions.Has(Permission::MERGE), PermissionLevel::NEUTRAL);
  ASSERT_EQ(permissions.Has(Permission::DELETE), PermissionLevel::GRANT);
  ASSERT_EQ(permissions.grants(),
            utils::UnderlyingCast(Permission::MATCH) |
                utils::UnderlyingCast(Permission::DELETE));
  ASSERT_EQ(permissions.denies(), utils::UnderlyingCast(Permission::CREATE));

  permissions.Revoke(Permission::DELETE);
  ASSERT_EQ(permissions.Has(Permission::MATCH), PermissionLevel::GRANT);
  ASSERT_EQ(permissions.Has(Permission::CREATE), PermissionLevel::DENY);
  ASSERT_EQ(permissions.Has(Permission::MERGE), PermissionLevel::NEUTRAL);
  ASSERT_EQ(permissions.Has(Permission::DELETE), PermissionLevel::NEUTRAL);
  ASSERT_EQ(permissions.grants(), utils::UnderlyingCast(Permission::MATCH));
  ASSERT_EQ(permissions.denies(), utils::UnderlyingCast(Permission::CREATE));
}

TEST(AuthWithoutStorage, PermissionsMaskTest) {
  Permissions p1(0, 0);
  ASSERT_EQ(p1.grants(), 0);
  ASSERT_EQ(p1.denies(), 0);

  Permissions p2(1, 0);
  ASSERT_EQ(p2.grants(), 1);
  ASSERT_EQ(p2.denies(), 0);

  Permissions p3(1, 1);
  ASSERT_EQ(p3.grants(), 0);
  ASSERT_EQ(p3.denies(), 1);

  Permissions p4(3, 2);
  ASSERT_EQ(p4.grants(), 1);
  ASSERT_EQ(p4.denies(), 2);
}

TEST(AuthWithoutStorage, UserSerializeDeserialize) {
  auto user = User("test");
  user.permissions().Grant(Permission::MATCH);
  user.permissions().Deny(Permission::MERGE);
  user.UpdatePassword("world");

  auto data = user.Serialize();

  auto output = User::Deserialize(data);
  ASSERT_EQ(user, output);
}

TEST(AuthWithoutStorage, RoleSerializeDeserialize) {
  auto role = Role("test");
  role.permissions().Grant(Permission::MATCH);
  role.permissions().Deny(Permission::MERGE);

  auto data = role.Serialize();

  auto output = Role::Deserialize(data);
  ASSERT_EQ(role, output);
}

TEST_F(AuthWithStorage, UserWithRoleSerializeDeserialize) {
  auto role = auth.AddRole("role");
  ASSERT_TRUE(role);
  role->permissions().Grant(Permission::MATCH);
  role->permissions().Deny(Permission::MERGE);
  auth.SaveRole(*role);

  auto user = auth.AddUser("user");
  ASSERT_TRUE(user);
  user->permissions().Grant(Permission::MATCH);
  user->permissions().Deny(Permission::MERGE);
  user->UpdatePassword("world");
  user->SetRole(*role);
  auth.SaveUser(*user);

  auto new_user = auth.GetUser("user");
  ASSERT_TRUE(new_user);
  ASSERT_EQ(*user, *new_user);
}

TEST_F(AuthWithStorage, UserRoleUniqueName) {
  ASSERT_TRUE(auth.AddUser("user"));
  ASSERT_TRUE(auth.AddRole("role"));
  ASSERT_FALSE(auth.AddRole("user"));
  ASSERT_FALSE(auth.AddUser("role"));
}

TEST(AuthWithoutStorage, Crypto) {
  auto hash = EncryptPassword("hello");
  ASSERT_TRUE(VerifyPassword("hello", hash));
  ASSERT_FALSE(VerifyPassword("hello1", hash));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  return RUN_ALL_TESTS();
}
#include "storage/indexes/impl/unique_ordered_index.hpp"

#include "database/db.hpp"

#include "database/db_accessor.hpp"
#include "database/db_transaction.hpp"
#include "storage/edge_accessor.hpp"
#include "storage/edge_record.hpp"
#include "storage/edge_type/edge_type.hpp"
#include "storage/label/label.hpp"
#include "storage/type_group_edge.hpp"
#include "storage/type_group_vertex.hpp"
#include "storage/vertex_accessor.hpp"
#include "storage/vertex_record.hpp"
#include "utils/iterator/iterator.hpp"

#include "storage/indexes/index_record.cpp"

template <class T, class K>
UniqueOrderedIndex<T, K>::UniqueOrderedIndex(Order order)
    : IndexBase<T, K>(true, order)
{
}

template <class T, class K>
UniqueOrderedIndex<T, K>::UniqueOrderedIndex(Order order,
                                             tx::Transaction const &t)
    : IndexBase<T, K>(true, order, t)
{
}

template <class T, class K>
bool UniqueOrderedIndex<T, K>::insert(IndexRecord<T, K> &&value)
{
    if (this->order() == Descending) {
        value.set_descending();
    }
    return set.access().insert(std::move(value)).second;
}

template <class T, class K>
std::unique_ptr<IteratorBase<const typename T::accessor_t>>
UniqueOrderedIndex<T, K>::for_range(DbAccessor &t, Border<K> from, Border<K> to)
{
    return std::make_unique<decltype(
        for_range_exact(t, std::move(from), std::move(to)))>(
        for_range_exact(t, std::move(from), std::move(to)));
}

template <class T, class K>
auto UniqueOrderedIndex<T, K>::for_range_exact(DbAccessor &t_v,
                                               Border<K> from_v, Border<K> to_v)
{
    auto acc = set.access();
    auto begin = acc.cbegin();
    auto end = to_v;

    // Sorted order must be checked
    if (this->order() == Ascending && from_v.key.is_present()) {
        begin = acc.cfind_or_larger(from_v);
    } else if (this->order() == Descending && to_v.key.is_present()) {
        begin = acc.cfind_or_larger(to_v);
        end = from_v;
    } else {
        assert(this->order() != None);
    }

    return iter::make_iterator([
        it = std::move(begin), b_end = std::move(end), t = t_v,
        hold_acc = std::move(acc)
    ]() mutable->auto {
        while (b_end >= it->key) {
            const IndexRecord<T, K> &r = *it;
            if (r.is_valid(t.db_transaction.trans)) {
                const typename T::accessor_t acc = r.access(t.db_transaction);
                it++;
                return make_option(std::move(acc));
            }
            it++;
        }

        return Option<const typename T::accessor_t>();
    });
}

template <class T, class K>
void UniqueOrderedIndex<T, K>::clean(DbTransaction &)
{
    // TODO: Actual cleaning
}

template class UniqueOrderedIndex<TypeGroupEdge, std::nullptr_t>;
template class UniqueOrderedIndex<TypeGroupVertex, std::nullptr_t>;

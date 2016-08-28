#include "storage/edge_accessor.hpp"

void EdgeAccessor::edge_type(const EdgeType &edge_type)
{
    this->record->data.edge_type = &edge_type;
}

const EdgeType &EdgeAccessor::edge_type() const
{
    runtime_assert(this->record->data.edge_type != nullptr, "EdgeType is null");
    return *this->record->data.edge_type;
}

const VertexAccessor EdgeAccessor::from() const
{
    return VertexAccessor(this->vlist->from(), this->db);
}

const VertexAccessor EdgeAccessor::to() const
{
    return VertexAccessor(this->vlist->to(), this->db);
}

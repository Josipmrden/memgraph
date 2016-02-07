#include <iostream>
#include <string>

#include "query_engine/i_code_cpu.hpp"

using std::cout;
using std::endl;

class CreateReturn : public ICodeCPU
{
public:

    std::string query() const override
    {
        return "CRETE RETURN QUERY";
    }

    void run(Db& db) const override
    {
        cout << db.identifier() << endl;
    }

    ~CreateReturn() {}
};

extern "C" ICodeCPU* produce() 
{
    return new CreateReturn();
}

extern "C" void destruct(ICodeCPU* p) 
{
    delete p;
}

#pragma once

class Header1
{
public:
    friend class TestClass;
    TestClass* GetTest()
    {
        return nullptr;
    }
};

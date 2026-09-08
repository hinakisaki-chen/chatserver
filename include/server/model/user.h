#pragma once

#include <string>

using namespace std;

// 位于数据层和业务层中间的ORM层，Object Relationan Mapping 对象关系映射
// 把数据库表变成代码里的类（对象），不用手写sql，用面向对象操作数据库

class User
{
public:
    User(int id = -1, string name = "", string password = "", string state = "offline")
    {
        this->id = id;
        this->name = name;
        this->password = password;
        this->state = state;
    }

    void setId(int id) { this->id = id; }
    void setName(string name) { this->name = name; }
    void setPassword(string password) { this->password = password; }
    void setState(string state) { this->state = state; }

    int getId() { return this->id; }
    string getName() { return this->name; }
    string getPassword() { return this->password; }
    string getState() { return this->state; }

protected:
    int id;
    string name;
    string password;
    string state;
};

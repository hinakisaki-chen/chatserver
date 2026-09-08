#pragma once

#include "user.h"

// 组和组内成员是多对多的关系，需要一张中间表表示他们之间所属关系
class GroupUser : public User
{
public:
    void setRole(string role) { this->role = role; }
    string getRole() { return this->role; }

private:
    string role;
};
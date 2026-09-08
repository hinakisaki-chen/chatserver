#pragma once

#include "user.h"

// user表的数据操作类
class UserModel
{
public:
    // user表的增加方法
    bool insert(User &user);

    // 根据用户提供的id查询信息
    User query(int id);

    // 更新用户状态信息
    bool updateState(User user);

    // 重置用户的状态信息
    void resetState();
};
#pragma once

/**
 * client和server的公共文件
 */
enum EnMsgType
{
    LOGIN_MSG = 1,
    LOGIN_MSG_ACK,
    LOGINOUT_MSG, // 注销消息
    REG_MSG,
    REG_MSG_ACK,
    ONE_CHAT_MSG,
    ADD_FRIEND_MSG, // 添加好友

    CREATE_GROUP_MSG,   // 创建群组
    ADD_GROUP_MSG,  // 加入群组
    GROUP_CHAT_MSG, // 新聊天

};
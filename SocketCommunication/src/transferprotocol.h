#ifndef TRANSFERPROTOCOL_H
#define TRANSFERPROTOCOL_H

#include <QByteArray>
#include <QString>
#include <QVector>

/**
 * @brief 自定义文件传输协议（核心：解决 TCP 粘包 / 拆包问题）
 *
 * 协议帧结构（发送端打包 / 接收端按此解包）：
 *
 *   ┌────────────┬────────────┬──────────────┬────────────┬──────────────┬─────────────┐
 *   │  magic     │  type      │  fileNameLen │  fileSize  │  fileName    │  fileData   │
 *   │ (4字节)    │ (4字节)    │  (4字节)     │  (8字节)   │  (变长)      │  (变长)     │
 *   └────────────┴────────────┴──────────────┴────────────┴──────────────┴─────────────┘
 *
 * 说明：
 *  - magic      固定魔数，用于校验数据是否对齐（防止读到错位数据时误解析）。
 *  - type       消息类型（当前仅文件类型，预留扩展）。
 *  - fileNameLen文件名（UTF-8 编码）的字节长度，用于精确切出文件名。
 *  - fileSize   文件数据总字节数，用于精确切出文件正文、判断是否收满。
 *
 * TCP 是"字节流"，不保证一次 write 对应一次 read，会出现：
 *   - 粘包：多个数据块在一次 read 里一起到达；
 *   - 拆包：一个数据块被拆成多次 read 到达。
 * 本协议通过"定长头 + 长度字段 + 缓冲状态机"精确切分每一帧，从而彻底解决该问题。
 *
 * 解析采用「流式事件」模型：文件不会整体攒在内存里，而是边到达边吐出事件：
 *   FileStarted（文件开始，携带文件名与总大小）
 *   → 若干次 FileData（每块数据最多 CHUNK 上限）
 *   → FileFinished（该文件收满）。
 * 接收方（FileReceiver）据此边收边写盘，内存占用与文件大小无关。
 */
class TransferProtocol
{
public:
    /// 协议魔数：任意固定值，只要收发两端一致即可
    static constexpr quint32 MAGIC = 0x46545453; // ASCII: "FTTS"

    /// 消息类型
    enum MessageType : quint32 {
        TypeFile = 1, ///< 文件消息（文本文件、图片文件统一走这一类型）
    };

    /// 协议头大小：magic(4) + type(4) + fileNameLen(4) + fileSize(8) = 20 字节
    static constexpr int HEADER_SIZE = 20;

    /// 单帧文件名的最大长度（防止异常数据导致超大内存分配）
    static constexpr quint32 MAX_FILE_NAME_LEN = 1024;

    /// 单个文件允许的最大大小（8GB，防御性上限）
    static constexpr qint64 MAX_FILE_SIZE = 8LL * 1024 * 1024 * 1024;

    /// 解析过程中吐出的事件类型
    enum class EventType {
        FileStarted,  ///< 一个新文件开始（header + 文件名解析完成）
        FileData,     ///< 文件数据块（本块可能只是文件的一部分）
        FileFinished, ///< 一个文件的数据全部收满
    };

    /// 一个解析事件
    struct Event {
        EventType  type = EventType::FileData;
        QString    fileName;   ///< FileStarted：文件名；其他事件为空
        qint64     fileSize = 0;       ///< FileStarted：文件总大小；FileData/FileFinished：本块/累计
        QByteArray data;       ///< FileData：本块数据；其他事件为空
        qint64     received = 0;       ///< FileData/FileFinished：已接收累计字节数
    };

public:
    TransferProtocol();

    /**
     * @brief 只打包「协议头 + 文件名」部分（不含文件正文）
     *
     * 供流式发送使用：发送端先写这一段，再分块续写文件正文。
     * 注意 fileSize 必须传真实文件大小（不能传 0），否则接收端
     * 会把后续正文误当成下一帧数据而丢弃。
     */
    static QByteArray buildHeader(MessageType type,
                                  const QString &fileName,
                                  qint64 fileSize);

    /**
     * @brief 把「类型 + 文件名 + 文件数据」打包成一帧完整的字节流
     * @param type     消息类型
     * @param fileName 文件名（会以 UTF-8 编码写入帧中）
     * @param fileData 文件二进制内容
     * @return 打包后的完整帧（协议头 + 文件名 + 文件数据），可直接交给 socket->write()
     */
    static QByteArray buildFrame(MessageType type,
                                 const QString &fileName,
                                 const QByteArray &fileData);

    /**
     * @brief 喂入新到达的字节，解析出事件流（处理粘包 / 拆包）
     *
     * 内部维护一个接收缓冲，采用状态机逐字节推进：
     *   1. WaitHeader    ：缓冲足够 20 字节则读头，校验魔数，记录文件名长度与文件大小，
     *                      吐出 FileStarted 事件；
     *   2. WaitFileName  ：缓冲足够 fileNameLen 字节则切出文件名；
     *   3. WaitData      ：缓冲里有正文数据就吐 FileData 事件（不攒整文件！），
     *                      收满 fileSize 字节后吐 FileFinished，回到 WaitHeader。
     * 任一步缓冲不足时立即返回，等待下一次 readyRead 继续喂入，绝不误切。
     *
     * @param chunk 本次 readyRead 读到的字节
     * @return 本次解析出的事件列表（可能为空，表示数据还不够切出任何事件）
     */
    QVector<Event> feed(const QByteArray &chunk);

    /// 当前是否正在接收一个文件（处于 FileStarted 与 FileFinished 之间）
    bool isReceivingFile() const;

    /**
     * @brief 复位解析状态（清空缓冲、回到 WaitHeader）
     *
     * 必须在以下时机调用，否则会出现"残留状态"bug：
     *   - 新连接接入时（旧连接可能是在收文件半途中断的）；
     *   - 传输被中止时（abortCurrentFile）。
     * 若不复位，新连接的数据会被当成"上一个没收完的文件正文"来解析，
     * 表现为"协议错误：收到数据块但未开始文件"。
     */
    void reset();

private:
    /// 解析状态
    enum class ParseState {
        WaitHeader,
        WaitFileName,
        WaitData,
    };

    QByteArray m_buffer;     ///< 未消费的接收缓冲（跨多次 readyRead 累积）
    ParseState m_state;      ///< 当前解析状态
    quint32     m_magic;     ///< 当前帧已读到的魔数
    quint32     m_type;      ///< 当前帧消息类型
    quint32     m_nameLen;   ///< 当前帧文件名长度
    qint64      m_fileSize;  ///< 当前帧文件数据长度
    QString     m_fileName;  ///< 当前帧已解析出的文件名
    qint64      m_received;  ///< 当前帧已接收的文件数据字节数
};

#endif // TRANSFERPROTOCOL_H

#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <QChar>
#include <QList>
#include <QString>

/**
 * @brief 表达式求值器（Model 层 / 计算核心）
 *
 * 【职责划分】
 *   本类只负责"字符串表达式 -> 数值结果"这一件事，不引用任何 Qt 界面类，
 *   也完全不知道 MainWindow 的存在。这样界面（View）与计算逻辑（Model）
 *   相互独立：Model 可以单独写单元测试，界面换成 Quick 或命令行也能复用。
 *
 * 【实现算法】
 *   递归下降解析器（Recursive Descent Parser），支持的文法如下：
 *
 *     expr    := term   ( ('+' | '-') term   )*
 *     term    := factor ( ('*' | '/') factor )*
 *     factor  := ('+' | '-') factor | primary     // 一元正负号
 *     primary := NUMBER | '(' expr ')'
 *
 *   该文法天然保证了：乘除优先于加减、同级左结合、括号可嵌套、支持 -3、-(1+2)。
 *
 * 【错误处理】
 *   不使用 C++ 异常，统一用返回值 + 错误串，避免异常穿过 Qt 事件循环。
 */
class Calculator
{
public:
    Calculator() = default;

    /**
     * @brief 对表达式求值
     * @param expression 输入表达式，允许含空格，允许使用 + - * / ( ) 与小数
     * @param result     求值成功时输出结果
     * @param error      求值失败时输出中文错误原因（供状态栏 / QMessageBox 显示）
     * @return true 成功；false 失败
     */
    bool evaluate(const QString &expression, double &result, QString &error) const;

    /**
     * @brief 判断某个字符是否是本计算器允许输入的合法字符
     *        供输入过滤（按钮生成、事件过滤器拦截非法按键）使用
     */
    static bool isAllowedChar(QChar c);

    /** @brief 判断字符是否为四则运算符 */
    static bool isOperator(QChar c);

private:
    // ---------- 词法单元 ----------
    enum class TokenType
    {
        Number,   // 数字字面量
        Plus,     // +
        Minus,    // -
        Multiply, // *
        Divide,   // /
        LParen,   // (
        RParen,   // )
        End       // 结束标记
    };

    struct Token
    {
        TokenType type  = TokenType::End;
        double    value = 0.0; // 仅 Number 类型有效
    };

    // ---------- 各阶段函数 ----------
    /** 词法分析：把字符串切成 Token 序列（末尾自动追加 End） */
    bool tokenize(const QString &expression, QList<Token> &tokens, QString &error) const;

    bool parseExpression(const QList<Token> &tokens, int &pos, double &value, QString &error) const;
    bool parseTerm(const QList<Token> &tokens, int &pos, double &value, QString &error) const;
    bool parseFactor(const QList<Token> &tokens, int &pos, double &value, QString &error) const;
    bool parsePrimary(const QList<Token> &tokens, int &pos, double &value, QString &error) const;
};

#endif // CALCULATOR_H

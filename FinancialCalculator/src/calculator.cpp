#include "calculator.h"

#include <QtGlobal> // qFuzzyIsNull

#include <cmath>

// ==============================================================================
//  对外接口
// ==============================================================================

bool Calculator::evaluate(const QString &expression, double &result, QString &error) const
{
    result = 0.0;
    error.clear();

    // 步骤一：空表达式快速失败
    if (expression.trimmed().isEmpty()) {
        error = QStringLiteral("表达式为空，请先输入算式");
        return false;
    }

    // 步骤二：词法分析（字符串 -> Token 序列）
    QList<Token> tokens;
    if (!tokenize(expression, tokens, error)) {
        return false;
    }

    // 步骤三：语法分析并同步求值（递归下降）
    int   pos   = 0;
    double value = 0.0;
    if (!parseExpression(tokens, pos, value, error)) {
        return false;
    }

    // 步骤四：整个 Token 序列必须被完整消费，否则说明存在多余符号
    if (tokens.at(pos).type != TokenType::End) {
        error = QStringLiteral("表达式格式错误：存在多余的符号 \"%1\"").arg(expression.trimmed());
        return false;
    }

    // 步骤五：结果有效性校验（防止 NaN / inf 污染界面显示）
    if (std::isnan(value) || std::isinf(value)) {
        error = QStringLiteral("计算结果溢出，请缩短算式或减小数值");
        return false;
    }

    result = value;
    return true;
}

bool Calculator::isAllowedChar(QChar c)
{
    // 允许空格：tokenize() 会跳过空白，让表达式可读性更好（如 "1 + 2"）
    return c.isDigit()
           || c == QLatin1Char('.')
           || c == QLatin1Char('+')
           || c == QLatin1Char('-')
           || c == QLatin1Char('*')
           || c == QLatin1Char('/')
           || c == QLatin1Char('(')
           || c == QLatin1Char(')')
           || c == QLatin1Char(' ');
}

bool Calculator::isOperator(QChar c)
{
    return c == QLatin1Char('+') || c == QLatin1Char('-')
           || c == QLatin1Char('*') || c == QLatin1Char('/');
}

// ==============================================================================
//  词法分析
// ==============================================================================

bool Calculator::tokenize(const QString &expression, QList<Token> &tokens, QString &error) const
{
    int       i = 0;
    const int n = expression.size();

    while (i < n) {
        const QChar c = expression.at(i);

        // 空白字符直接跳过
        if (c.isSpace()) {
            ++i;
            continue;
        }

        // ---- 数字（含小数） ----
        if (c.isDigit() || c == QLatin1Char('.')) {
            const int start    = i;
            int       dotCount = (c == QLatin1Char('.')) ? 1 : 0;
            ++i;

            while (i < n) {
                const QChar d = expression.at(i);
                if (d.isDigit()) {
                    ++i;
                } else if (d == QLatin1Char('.')) {
                    ++dotCount;
                    if (dotCount > 1) {
                        // 例如 "1.2.3"
                        error = QStringLiteral("数字格式错误：同一个数字中出现了多个小数点");
                        return false;
                    }
                    ++i;
                } else {
                    break;
                }
            }

            const QString numStr = expression.mid(start, i - start);
            if (numStr == QLatin1String(".")) {
                // 例如 "1+."
                error = QStringLiteral("数字格式错误：小数点前后缺少数字");
                return false;
            }

            bool   ok = false;
            const double v = numStr.toDouble(&ok);
            if (!ok) {
                error = QStringLiteral("数字格式错误：无法识别 \"%1\"").arg(numStr);
                return false;
            }

            tokens.append(Token{TokenType::Number, v});
            continue;
        }

        // ---- 运算符与括号 ----
        switch (c.unicode()) {
        case '+': tokens.append(Token{TokenType::Plus, 0.0}); break;
        case '-': tokens.append(Token{TokenType::Minus, 0.0}); break;
        case '*': tokens.append(Token{TokenType::Multiply, 0.0}); break;
        case '/': tokens.append(Token{TokenType::Divide, 0.0}); break;
        case '(': tokens.append(Token{TokenType::LParen, 0.0}); break;
        case ')': tokens.append(Token{TokenType::RParen, 0.0}); break;
        default:
            // 出现中文、字母等非法字符
            error = QStringLiteral("非法字符 \"%1\"，本计算器仅支持 0-9 . + - * / ( )").arg(c);
            return false;
        }
        ++i;
    }

    if (tokens.isEmpty()) {
        error = QStringLiteral("表达式为空，请先输入算式");
        return false;
    }

    tokens.append(Token{TokenType::End, 0.0}); // 结束哨兵
    return true;
}

// ==============================================================================
//  语法分析（自顶向下，边解析边计算）
// ==============================================================================

// expr := term ( ('+'|'-') term )*
bool Calculator::parseExpression(const QList<Token> &tokens, int &pos, double &value, QString &error) const
{
    if (!parseTerm(tokens, pos, value, error)) {
        return false;
    }

    while (tokens.at(pos).type == TokenType::Plus || tokens.at(pos).type == TokenType::Minus) {
        const TokenType op = tokens.at(pos).type;
        ++pos;

        double rhs = 0.0;
        if (!parseTerm(tokens, pos, rhs, error)) {
            return false;
        }

        value = (op == TokenType::Plus) ? (value + rhs) : (value - rhs);
    }
    return true;
}

// term := factor ( ('*'|'/') factor )*
bool Calculator::parseTerm(const QList<Token> &tokens, int &pos, double &value, QString &error) const
{
    if (!parseFactor(tokens, pos, value, error)) {
        return false;
    }

    while (tokens.at(pos).type == TokenType::Multiply || tokens.at(pos).type == TokenType::Divide) {
        const TokenType op = tokens.at(pos).type;
        ++pos;

        double rhs = 0.0;
        if (!parseFactor(tokens, pos, rhs, error)) {
            return false;
        }

        if (op == TokenType::Multiply) {
            value *= rhs;
        } else {
            // ---------- 除零检查：这是本题明确要求处理的错误分支 ----------
            if (qFuzzyIsNull(rhs)) {
                error = QStringLiteral("数学错误：除数不能为零");
                return false;
            }
            value /= rhs;
        }
    }
    return true;
}

// factor := ('+'|'-') factor | primary
bool Calculator::parseFactor(const QList<Token> &tokens, int &pos, double &value, QString &error) const
{
    // 一元正负号，可连续出现，例如 "--3"、"-(1+2)"
    if (tokens.at(pos).type == TokenType::Plus || tokens.at(pos).type == TokenType::Minus) {
        const bool negative = (tokens.at(pos).type == TokenType::Minus);
        ++pos;

        if (!parseFactor(tokens, pos, value, error)) {
            return false;
        }
        if (negative) {
            value = -value;
        }
        return true;
    }

    return parsePrimary(tokens, pos, value, error);
}

// primary := NUMBER | '(' expr ')'
bool Calculator::parsePrimary(const QList<Token> &tokens, int &pos, double &value, QString &error) const
{
    const Token &t = tokens.at(pos);

    if (t.type == TokenType::Number) {
        value = t.value;
        ++pos;
        return true;
    }

    if (t.type == TokenType::LParen) {
        ++pos;
        if (!parseExpression(tokens, pos, value, error)) {
            return false;
        }
        if (tokens.at(pos).type != TokenType::RParen) {
            error = QStringLiteral("括号不匹配：缺少右括号 \')\'");
            return false;
        }
        ++pos;
        return true;
    }

    // 运行到这里说明表达式不完整，给出针对性的中文提示
    switch (t.type) {
    case TokenType::End:
        error = QStringLiteral("表达式不完整：运算符后面缺少运算数");
        break;
    case TokenType::RParen:
        error = QStringLiteral("括号不匹配：出现了多余的右括号 \')\'");
        break;
    case TokenType::Multiply:
    case TokenType::Divide:
        error = QStringLiteral("格式错误：乘号或除号的位置不正确");
        break;
    default:
        error = QStringLiteral("表达式格式错误：此处应当是一个数字");
        break;
    }
    return false;
}

#include "financecalculator.h"

#include <cmath>

FinanceCalculator::Result FinanceCalculator::compute(double       principal,
                                                     double       annualRatePercent,
                                                     int          years,
                                                     InterestMode mode,
                                                     QString     *error)
{
    Result result;

    // 统一的失败出口：写错误信息并返回全零结果
    auto fail = [&error, &result](const QString &message) -> Result {
        if (error) {
            *error = message;
        }
        return result;
    };

    // ---------- 参数合法性校验（对应题目"错误处理"要求） ----------
    if (principal < 0.0) {
        return fail(QStringLiteral("本金不能为负数"));
    }
    if (years <= 0) {
        return fail(QStringLiteral("年限必须大于 0"));
    }
    if (annualRatePercent < 0.0) {
        return fail(QStringLiteral("年利率不能为负数"));
    }

    // 百分数 -> 小数：3.5% => 0.035
    const double rate = annualRatePercent / 100.0;

    // ---------- 核心公式 ----------
    if (mode == InterestMode::Simple) {
        // 单利：FV = P × (1 + r × n)
        result.futureValue = principal * (1.0 + rate * static_cast<double>(years));
    } else {
        // 复利：FV = P × (1 + r) ^ n
        result.futureValue = principal * std::pow(1.0 + rate, static_cast<double>(years));
    }

    result.interest = result.futureValue - principal;

    // ---------- 结果有效性校验：防止超大年限/利率导致溢出 ----------
    if (std::isnan(result.futureValue) || std::isinf(result.futureValue)) {
        return fail(QStringLiteral("计算结果溢出，请降低年利率或年限"));
    }

    return result;
}

QString FinanceCalculator::modeName(InterestMode mode)
{
    return (mode == InterestMode::Simple) ? QStringLiteral("单利") : QStringLiteral("复利");
}

QString FinanceCalculator::formulaText(double       principal,
                                       double       annualRatePercent,
                                       int          years,
                                       InterestMode mode)
{
    const double rate = annualRatePercent / 100.0;

    if (mode == InterestMode::Simple) {
        return QStringLiteral("单利公式：FV = P × (1 + r × n)\n"
                              "　　　　　= %1 × (1 + %2 × %3)")
            .arg(QString::number(principal, 'f', 2))
            .arg(QString::number(rate, 'f', 4))
            .arg(years);
    }

    return QStringLiteral("复利公式：FV = P × (1 + r) ^ n\n"
                          "　　　　　= %1 × (1 + %2) ^ %3")
        .arg(QString::number(principal, 'f', 2))
        .arg(QString::number(rate, 'f', 4))
        .arg(years);
}

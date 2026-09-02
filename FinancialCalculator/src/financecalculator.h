#ifndef FINANCECALCULATOR_H
#define FINANCECALCULATOR_H

#include <QString>

/**
 * @brief 计息方式
 *        单利：利息不再生息；复利：利息滚入下期本金（俗称"利滚利"）
 */
enum class InterestMode
{
    Simple,   // 单利
    Compound  // 复利
};

/**
 * @brief 财务计算器（Model 层）
 *
 * 【职责划分】
 *   同样属于 Model 层，只做纯粹的数学计算，不含任何界面代码。
 *   主窗口通过对话框拿到参数后，把参数交给本类计算，再把结果交给界面显示。
 *
 * 【计算公式】
 *   单利终值：FV = P × (1 + r × n)
 *   复利终值：FV = P × (1 + r) ^ n
 *   其中 P = 本金，r = 年利率（小数形式，内部由百分数换算而来），n = 年限
 */
class FinanceCalculator
{
public:
    /** @brief 一次财务计算的完整结果 */
    struct Result
    {
        double futureValue = 0.0; // 本息合计（终值 FV）
        double interest    = 0.0; // 利息 = 终值 - 本金
    };

    /**
     * @brief 执行财务计算
     * @param principal          本金
     * @param annualRatePercent  年利率（百分数形式，例如 3.5 表示 3.5%）
     * @param years              年限（必须大于 0）
     * @param mode               计息方式
     * @param error              可选：出错时写入中文错误原因
     * @return 计算结果；若参数非法，返回全零的 Result
     */
    static Result compute(double       principal,
                          double       annualRatePercent,
                          int          years,
                          InterestMode mode,
                          QString     *error = nullptr);

    /** @brief 计息方式的中文名称，用于界面显示 */
    static QString modeName(InterestMode mode);

    /** @brief 生成一段中文公式说明，用于结果弹窗 */
    static QString formulaText(double       principal,
                               double       annualRatePercent,
                               int          years,
                               InterestMode mode);
};

#endif // FINANCECALCULATOR_H

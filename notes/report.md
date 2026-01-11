<div
    style="
        height: 90vh;
        width: 100%;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: space-between;
        text-align: center;
    ">
    <div style="padding-top: 60px">
        <img src="./report.assets/PKU_Logo.svg" width="220px" />
    </div>
    <div style="display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 30px; text-align: center">
        <div style="text-align: center; font-size: 36px; font-weight: bold; line-height: 1.4;">
            基于 xv6-k210 改造的<br />
            xv6-riscv 操作系统
        </div>
        <p style="font-size: 18px; margin: 0">北京大学 2025 秋季学期操作系统课程 Lab 实验报告</p>
    </div>
    <div style="padding-bottom: 60px">
        <p style="font-size: 18px; margin: 0">2110306206 卓致用</p>
    </div>
</div>
<div style="page-break-after: always"></div>
# 前言

本文内容极度冗长，因为是完全是对于当时写 Lab 时所做笔记的简单拼接，包含了很多不必要的操作/测例说明与完整的代码修改，但确实没有精力去进行精简了，请助教见谅。

本文亦可以直接访问我的博客在线阅读：[Arthals' Ink / #xv6](https://arthals.ink/tags/xv6)。

本实验的全部代码开源在 [zhuozhiyongde/XV6-OS-2025Fall-PKU](https://github.com/zhuozhiyongde/XV6-OS-2025Fall-PKU)。

对实验的意见或建议：

1. 建议对 Lab 进行一些完善优化操作，其实可以彻底提供一个更干净的起点仓库，彻底移除 k210 相关的代码和信息；文档也建议提供更多的上下文和操作示例，包括对于 xv6 的一些必要讲解等。
2. 优化希冀平台的测试流程，尤其是对于 `init` 的加入，希望不要一直硬编码拉起代码（这点在本仓库和笔记中都提及修改方法）。
3. 对于不同的 part 现在的 git 历史非常混乱，完全没有历史树，其实功能性 part 彼此之间完全可以有一个更简明、有序的提交历史（比如我仓库里的 Prepare 提交），从而可以让大家进行 cherry pick 操作，一直在一个分支里进行逐 part 开发，而不是每做一个功能性 part 就新开一个分支。

对课程的意见或建议：

1. 主要是建议优化一下 PPT 内容，现在的信息密度比较低而且排版不是很美观，学习/复习的时候不是很方便
2. 希望期末也能开卷，且期末考试不要考太过细节的内容，感觉不是很有必要

本文其余内容同 `notes/` 目录下内容一致，在此不再附加全文，节省篇幅。
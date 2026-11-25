/*
 * OctiRulebookI18n.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiRulebook.hxx"
#include <cctype>
#include <sstream>

namespace octi {

namespace {

struct LocalizedRulebookPack {
    const char *lang;
    std::vector<RulebookSection> sections;
    const char *docTitle;
    const char *bannerSub;
    const char *chaptersLabel;
    const char *ch1Heading;
    const char *ch1Intro;
    const char *ch1GlossPod;
    const char *ch1GlossProng;
    const char *ch1GlossSuperProng;
    const char *ch1GridBullet;
    const char *ch1ResBullet;
    const char *ch1CompassBullet;
    const char *fig1Caption;
    const char *fig2Caption;
    const char *fig3Caption;
    const char *ch2Heading;
    const char *ch2Intro;
    const char *ch2Act1;
    const char *ch2Act2;
    const char *ch2Act3;
    const char *ch2Act4;
    const char *ch2CaptureBox;
    const char *fig4Caption;
    const char *fig5Caption;
    const char *fig7Caption;
    const char *ch3Heading;
    const char *ch3StackBullet;
    const char *ch3CaptureBullet;
    const char *ch3LoopBullet;
    const char *ch3LibBullet;
    const char *ch3EdgeBullet;
    const char *ch3SuperBullet;
    const char *fig6Caption;
    const char *fig8Caption;
    const char *fig9Caption;
    const char *ch4Heading;
    const char *ch4Win3b;
    const char *ch4Win1b;
    const char *ch4WinOfk;
    const char *ch4Win4p;
    const char *ch4Elim4p;
    const char *ch4Stalemate;
    const char *ch4NoProgress;
    const char *fig10Caption;
    const char *fig11Caption;
    const char *fig12Caption;
    const char *ch5Heading;
    const char *ch5Intro;
    const char *ch5ThCode;
    const char *ch5ThGrid;
    const char *ch5ThPlayers;
    const char *ch5ThPods;
    const char *ch5ThProngs;
    const char *ch5ThMod;
    const char *ch5ThRated;
    const char *ch5Yes;
    const char *ch5Casual;
    const char *ch5ModStd3b;
    const char *ch5ModSp3b;
    const char *ch5ModEd3b;
    const char *ch5ModSped3b;
    const char *ch5ModBlitz1b;
    const char *ch5ModSp1b;
    const char *ch5ModEd1b;
    const char *ch5ModSped1b;
    const char *ch5Mod4p;
    const char *ch5Mod4pSp;
    const char *ch5Mod4pEd;
    const char *ch5Mod4pSped;
    const char *ch5ModOfk;
    const char *ch6Heading;
    const char *ch6GrammarSub;
    const char *ch6ThSyntax;
    const char *ch6ThAction;
    const char *ch6ThExample;
    const char *ch6ActDeploy;
    const char *ch6ActLiberate;
    const char *ch6ActAddNormal;
    const char *ch6ActAddSuper;
    const char *ch6ActStep;
    const char *ch6ActJump;
    const char *ch6ActComposite;
    const char *ch6ActPrelude;
    const char *ch6FenSub;
    const char *ch6FenIntro;
    const char *ch6FenB1;
    const char *ch6FenB2;
    const char *ch6FenB3;
    const char *ch6FenB4;
    const char *ch6FenB5;
    const char *ch6FenB6;
    const char *ch7Heading;
    const char *ch7Intro;
    const char *ch7Pool1Box;
    const char *ch7Pool2Box;
};

void appendI18nFigureTable(std::ostringstream &os,
                           const char *fileName,
                           const char *caption,
                           const char *figBg,
                           const char *borderHex,
                           const char *dimHex)
{
    os << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"8\" "
       << "style=\"margin-top:8px;margin-bottom:12px;border:1px solid " << borderHex
       << ";background-color:" << figBg << ";\">"
       << "<tr><td align=\"center\">"
       << "<img src=\"octi://rulebook/" << fileName << "\" width=\"460\" height=\"460\"><br>"
       << "<span style=\"font-size:12px;color:" << dimHex << ";\">"
       << caption << "</span>"
       << "</td></tr></table>\n";
}

const std::vector<LocalizedRulebookPack> &localizedPacks()
{
    static const std::vector<LocalizedRulebookPack> kPacks = {
        /* 1. Traditional Chinese (Taiwan, zh-TW) */
        {
            "zh-TW",
            {
                { "ch1", "第 1 章", "棋盤幾何、Pod、Prong 與初始配置", "9x9 座標、雙方起始預備區與 a-h 八方位插孔設定" },
                { "ch2", "第 2 章", "完整回合行動", "插入 Prong、單格步進、多段連跳、部署預備區 Pod 與解救" },
                { "ch3", "第 3 章", "堆疊、組合走法與附錄 A 特殊情況", "同格多 Pod 逗號分進、A.1 迴圈跳回、無邊界環繞與 SuperProng" },
                { "ch4", "第 4 章", "勝利條件與 4P 基地淘汰", "3 基地與 1 基地佔領勝利、4P 淘汰及 30／60 手無進展和局" },
                { "ch5", "第 5 章", "13 種標準規則變體對照表", "棋盤尺寸、Pod／Prong 配額、特殊修正與計分對局資格" },
                { "ch6", "第 6 章", "官方棋譜記號與 OCTI-FEN 語法", "代數走法語法與 11／13 欄位 OCTI-FEN 盤面格式" },
                { "ch7", "第 7 章", "雙池 Elo 等級分與競技場規則", "純人類排名賽（第 1 池）與開放機器人競技場（第 2 池）規範" }
            },
            "OCTI 官方圖解規則書",
            "涵蓋完整多變體對局規則、戰術機制、附錄 A 特殊情況、官方棋譜記號／OCTI-FEN 語法與雙池 Elo 競技規範。",
            "章節導覽：",
            "第 1 章 &mdash; 棋盤幾何、Pod、Prong 與初始配置",
            "OCTI 是一款完全資訊、零運氣的向量建構抽象策略棋盤遊戲。每顆 Pod 進場時皆無任何移動方向，玩家透過每回合插入 Prong 逐步建立八方移動向量。",
            "Pod（八角形主棋子，周圍有 8 個方向插孔）",
            "Prong（插入 Pod 插孔以賦予移動方向的方向插銷）",
            "SuperProng（金色的特殊 Prong，每支可在正規行動前提供一次直線獎勵移動）",
            "<b>9&times;9 標準棋盤：</b>直行 <code>x = 1..9</code> 由西向東（左至右），橫列 <code>y = 1..9</code> 由南向北（下至上），每格以兩位數 <code>xy</code> 表示（如 <code>53</code> 為第 5 直行、第 3 橫列）。",
            "<b>雙人對局初始資源（<code>2P_3B</code> 與 <code>2P_1B</code>）：</b>A 方於本陣基地 <code>33</code>、<code>53</code>、<code>73</code> 各置 1 顆 Pod（共 3 顆），B 方於 <code>37</code>、<code>57</code>、<code>77</code> 各置 1 顆 Pod；每方另持 <b>4 顆預備區 Pod</b> 與 <b>25 支 Prong</b>（<code>_SP</code> 變體為 24 支一般 Prong + 1 支金色 SuperProng）。",
            "<b>8 方位插孔（<code>a</code>&ndash;<code>h</code>）：</b>由北方順時針依序為 <code>a</code>=N、<code>b</code>=NE、<code>c</code>=E、<code>d</code>=SE、<code>e</code>=S、<code>f</code>=SW、<code>g</code>=W、<code>h</code>=NW。",
            "圖 1：標準 9x9 雙人對局配置（2P_3B），本陣基地為 33/53/73 對決 37/57/77。",
            "圖 2：2P_1B（單基地快棋）採用相同預設棋盤設定，但只要佔領任 1 座敵方基地（55ab-57x）即獲勝。",
            "圖 3：Pod A55acg 周圍 a-h 八方位插孔與插入新 Prong（55acg+b）示意。",
            "第 2 章 &mdash; 完整回合行動",
            "A 方先手，雙方輪流行動（<code>4P</code> 依 <code>A &rarr; B &rarr; C &rarr; D</code> 輪轉並跳過已淘汰座位）。禁止空過，每回合必須執行<b>恰好一項</b>正規行動：",
            "<b>插入 Prong（<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>）：</b>從預備區取 1 支 Prong 插入棋盤上己方 Pod 的空插孔（如 <code>55acg+b</code> 或 SuperProng <code>33+b!</code>）。Prong 一旦插入即永久固定，任何變體皆禁止拔出或換位。",
            "<b>單格步進（<code>xy&lt;cfg&gt;-x'y'</code>）：</b>將一顆 Pod 沿其已安裝之 Prong 方向移動 1 格至空格或你自己的 Pod 堆疊（如 <code>44ab-45</code>）。單格步進<b>絕不俘獲棋子</b>，亦不可進入有其他玩家 Pod 的棋格（包含 <code>4P</code> 中的對角隊友）。",
            "<b>雙格跳躍與多段連跳（<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>）：</b>沿已安裝之 Prong 方向跳過相鄰的有子格（敵我皆可），落入其後的空格；同一顆 Pod 可沿其任一已安裝的 Prong 轉向連跳，也可在任一次合法跳躍後停止。",
            "<b>部署預備區 Pod（<code>xy</code>）或解救被俘 Pod（<code>xy*</code>）：</b>當你自己的本陣基地為空格或僅有你自己的 Pod 時，放入 1 顆預備區 Pod（或放入你已佔領之敵方基地）；於 <code>2P_3B*</code> 及 <code>4P*</code> 對局中，符合資格時可在你已佔領之 OCTI 基地解救 1 顆你自己的被俘 Pod（<code>xy*</code>；解救不適用於單基地驟死變體與 <code>OFK</code>）。",
            "<b>選擇性整疊俘獲與 Prong 奪取：</b>跳過有子格時可選擇附加 <code>x</code>（如 <code>44ab-66x</code>）將該格<b>整疊所有 Pod</b> 一併俘獲（禁止部分俘獲），省略 <code>x</code>（如 <code>44ab-66</code>）則保留該堆疊；在一般單一走法與組合走法中，被俘堆疊於回合結束時移入各 Pod 原主人的 <code>capturedPods</code> 池，其上所有 Prong 則轉入俘獲方的可用 Prong 預備區。",
            "圖 4：Pod A44ab 向北步進（44ab-45）與向東北跳躍俘獲整疊 B55e（44ab-66x）。",
            "圖 5：多段連跳 33abd-55-57x-75x 越過友方 A44h 後轉向連續俘獲敵子。",
            "圖 7：於空基地 33 部署預備區 Pod，以及在 2P_3B* 與 4P* 中解救被俘 Pod（53* 或 57*）。",
            "第 3 章 &mdash; 堆疊、組合走法與附錄 A 特殊情況",
            "<b>自身 Pod 堆疊與多 Pod 組合走法（<code>,</code>）：</b>多顆屬於你自己顏色的 Pod 可堆疊於同一格（<code>OFK</code> 禁止堆疊；在 <code>4P</code> 中，嚴禁與其他顏色——包含你的對角隊友——堆疊在同一格）。<b>回合開始時位於同一格</b>的己方 Pod，可任選其中幾顆於同一回合一起移動，或沿各自的步進／跳躍路徑分進，以逗號串接（如 <code>44a-45,44beg-34</code>）。放置 Pod 與插入 Prong 絕不可與移動合併。",
            "<b>整疊俘獲與回合結束移除：</b>在一般單一走法與組合走法中，以 <code>x</code> 跳過的堆疊在同一回合內仍留在棋盤上，可作為同疊其餘 Pod 進行組合走法時的跳板，於回合結束時統一移除；但單一 Pod 在同一連跳鏈中禁止跳過同一格兩次。",
            "<b>附錄 A.1 &mdash; 起點格淨空、迴圈跳回與同疊 Pod 不可互跳：</b>跳躍中的 Pod 禁止跳過自己的起點格 <code>S</code>，也不可跳過回合開始時與其同疊的 Pod（故無邊界 <code>44bcd-66-86-16-34-54</code> 跳過留在 <code>44</code> 的 <code>A44a</code> 屬違例）；且唯有當回合開始時位於 <code>S</code> 的所有 Pod 均離開 <code>S</code> 時，連跳終點才可落回 <code>S</code>（如 <code>44a-45,44beg-66x-64x-44</code> 合法）。",
            "<b>解救規則適用範圍（<code>2P_3B*</code> 與 <code>4P*</code>）：</b><code>xy*</code> 僅適用於 3 基地與 4P 變體，且目標基地必須已有至少 1 顆己方 Pod 駐守。",
            "<b>無邊界環繞（<code>_ED</code>）：</b>第 1 直行與第 9 直行相連、第 1 橫列與第 9 橫列相連（含對角方向，如步進 <code>15g-95</code> 與跨界跳躍俘獲 <code>97b-29x</code>）。",
            "<b>SuperProng 直線獎勵前奏（<code>_SP</code>，<code>;</code>）：</b>正規行動前，棋盤上每控制 1 支 SuperProng 可執行 <b>1 次直線獎勵前奏移動</b>（俘獲敵方 SuperProng 後，<code>2P_SP</code> 最多 2 次、<code>4P_SP</code> 最多 4 次，如 <code>44ab!-66x;66ab!-67</code>），前奏中被 <code>x</code> 俘獲的堆疊會立即移除，勝利條件於完整回合結束時判定。",
            "圖 6：堆疊 [A44a, A44beg] 分進（44a-45,44beg-34）或完全清空 44 後迴圈落回 44（44a-45,44beg-66x-64x-44）。",
            "圖 8：無邊界（_ED）跨邊步進 15g-95 與對角跨邊跳躍俘獲 97b-29x。",
            "圖 9：SuperProng 直線前奏 44ab!-66x; 立即移除被俘子，再執行正規步進 66ab!-67。",
            "第 4 章 &mdash; 勝利條件、4P 基地淘汰與無進展和局",
            "<b><code>2P_3B</code> 系列（3 基地標準賽）：</b>回合結束時同時佔領敵方<b>全部 3 座</b>本陣基地即獲勝。",
            "<b><code>2P_1B</code> 系列（1 基地快棋）：</b>回合結束時佔領敵方<b>任 1 座</b>本陣基地即獲勝。",
            "<b><code>OFK</code>（OCTI for Kids，6&times;7）：</b>回合結束時佔領敵方 4 座基地中的<b>任 1 座</b>即獲勝。",
            "<b><code>4P</code> 聯隊系列（A&amp;C 隊對 B&amp;D 隊）：</b>回合結束時，同隊夥伴同時佔領敵隊<b>兩座角落基地</b>即共同獲勝。",
            "<b>4P 基地永久淘汰與隊友基地安全規則：</b>在 <code>4P</code> 中，若<b>敵方</b> Pod 於完整回合結束時停在你的角落本陣基地上（或你的所有 Pod 均被俘獲且預備區已無剩餘 Pod），你將被<b>永久淘汰</b>：場上殘存 Pod 全數移入你的 <code>capturedPods</code> 池，且<b>其上所有 Prong（一般 Prong 與 SuperProng）全部轉交給佔領基地的敵方玩家</b>。已淘汰座位在輪轉時會跳過，由倖存隊友繼續作戰。相對地，<b>步進或跳躍進入對角隊友空著（或僅有你自己的 Pod 進駐）的起始基地完全合法且無任何後果——絕不會淘汰你的隊友</b>（若你場上無任何 Pod 時隊友暫時停留在你的起始基地上，則只會跳過你的回合，直到隊友離開該基地為止）。",
            "<b>全滅與無子可動：</b>輪到行動方若無任何合法走法，剛完成行動的對手（或敵隊）立即獲勝。",
            "<b>無進展和局（<code>NO_PROGRESS_DRAW</code>）：</b><code>2P*</code>／<code>OFK</code> 連續 <b>30 手</b>（<code>4P*</code> 連續 <b>60 手</b>）無任何俘獲、部署／解救 Pod、插入 Prong 或 4P 淘汰時，自動裁定和局。",
            "圖 10：4P 聯隊模式（4P）：對角盟友 A(33)&C(77) 對抗 B(37)&D(73)、同色堆疊 [A55ab, A55f]、隊友 Pod C65cf、安全進入隊友基地與中立格 53／57。",
            "圖 11：6x7 OFK 模式：每方 4 座基地、無預備區 Pod、禁止堆疊、佔領 1 座敵方基地即獲勝（34a-36x）。",
            "圖 12：2P_3B 勝利盤面（status = O），A 方同時佔領全部 3 座敵方基地（A37c, A57a, A77g）。",
            "第 5 章 &mdash; 13 種標準規則變體對照表",
            "所有平台（網頁、桌面版、Android、棋譜庫與機器人 SDK）統一支援下列 <b>13 種標準變體代碼</b>：",
            "代碼", "棋盤", "人數", "Pod（起始+預備）", "Prong（一般／SP）", "規則特性與勝利目標", "計分資格",
            "計分", "休閒",
            "標準規則；同時佔領全部 3 座敵方基地",
            "SuperProng <code>!</code> + <code>;</code> 直線前奏（&le;2）；3 座基地",
            "無邊界環繞棋盤；3 座基地",
            "SuperProng（&le;2 前奏）+ 無邊界；3 座基地",
            "驟死快棋；佔領任 1 座敵方基地",
            "SuperProng（&le;2 前奏）+ 1 基地驟死賽",
            "無邊界 + 1 基地驟死賽",
            "SuperProng（&le;2 前奏）+ 無邊界 + 1 基地",
            "中立格 <code>53/57</code>、基地永久淘汰；2 座敵隊角落基地",
            "4P 聯隊 + SuperProng 直線前奏（&le;4）",
            "4P 聯隊 + 無邊界環繞棋盤",
            "4P 聯隊 + SuperProng（&le;4 前奏）+ 無邊界",
            "OCTI for Kids：每方 4 座基地、無預備區、禁止堆疊與組合走法；佔領 1 座基地即獲勝",
            "第 6 章 &mdash; 官方棋譜記號與 OCTI-FEN 規格",
            "走法記號語法表",
            "語法", "行動說明", "範例",
            "於 OCTI 基地格部署 1 顆預備區 Pod",
            "於已佔領之 OCTI 基地格解救己方被俘 Pod（僅限 <code>2P_3B*</code> 與 <code>4P*</code>）",
            "於插孔 <code>d &isin; {a..h}</code> 插入一般 Prong",
            "於插孔 <code>d</code> 插入金色 SuperProng",
            "不俘獲的單格步進",
            "單段或多段連跳（<code>x</code> = 選擇性整疊俘獲）",
            "從同一起點格分進多顆 Pod 的組合走法",
            "1&ndash;4 次直線 SuperProng 獎勵前奏（棋盤上每支 SP 限 1 次；被 <code>x</code> 俘獲者立即移除）+ 正規行動",
            "OCTI-FEN 盤面字串格式",
            "盤面狀態以冒號分隔序列化為 11 或 13 個欄位（可選附加第 12／14 個欄位 <code>noProgressPlies</code>）：",
            "<code>mode</code>：<code>2</code>、<code>4</code> 或 <code>OFK</code> &bull; <code>bases</code>：<code>3</code> 或 <code>1</code> &bull; <code>super</code> / <code>edgeless</code>：<code>0</code> 或 <code>1</code>。",
            "<code>status</code>：<code>N</code>（未開始）、<code>P</code>（進行中）、<code>O</code>（對局結束）、<code>R</code>（認輸）。",
            "<code>turn</code> / <code>winner</code>：<code>A</code>、<code>B</code>、<code>C</code>、<code>D</code> 或 <code>-</code> &bull; <code>elimMask</code>：4P 淘汰位元遮罩（<code>A=1, B=2, C=4, D=8</code>）。",
            "<code>resP</code>：<code>freePods,capturedPods,freeProngs,freeSuperProngs</code>（例如 <code>4,0,25,0</code>）。",
            "<code>squares</code>：依橫列優先順序以逗號分隔之棋盤 Pod <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code>（如 <code>A44ab!,B55e!</code>），空盤為 <code>-</code>。",
            "<code>noProgressPlies</code>（可選的第 12／14 個尾端欄位）：連續無進展手數（<code>0..1000</code>，省略時預設為 <code>0</code>；標準 <code>toPositionString()</code> 輸出 11／13 個欄位的字串）。",
            "第 7 章 &mdash; 雙池 Elo 等級分與競技場規則",
            "官方等級分起始為 <b>1200 Elo</b>，分別計算於三項獨立的 1v1 變體排名賽：<b><code>2P_3B</code></b>、<b><code>2P_1B</code></b> 與 <b><code>OFK</code></b>。所有 4P、SuperProng、無邊界、混合變體及離線對局均為休閒（不計分）對局。",
            "<b>第 1 池 &mdash; 純人類排名賽（排行榜與玩家名錄）：</b><br>僅計入人類對人類之線上計分對局（標準係數 <code>K = 32</code>），完全不受機器人對局影響。",
            "<b>第 2 池 &mdash; 開放機器人與人類競技場（排行榜與機器人名錄）：</b><br>涵蓋至少一方為註冊 AI 機器人之線上計分對局，並設有三大防刷分保護機制：<ul><li><b>人類對機器人：</b>採用標準 <code>K = 32</code>，場次無上限（包含對戰自己的機器人）。</li><li><b>機器人對機器人 K 值：</b>不同擁有者之間的對局採用 <code>K = 32</code>；同一擁有者的自我測試降為 <code>K = 16</code>。</li><li><b>配對上限與提早認輸保護：</b>同一組機器人配對（不分先後手）每滾動 24 小時最多計分 <b>4 局</b>（第 5 局起自動轉為休閒局；對戰不同機器人則無每日上限），且未滿 <b>8 手</b>即認輸者不計 Elo 增減。</li></ul>"
        },
        /* 2. Simplified Chinese (Mainland China, zh-CN) */
        {
            "zh-CN",
            {
                { "ch1", "第 1 章", "棋盘几何、Pod、Prong 与开局设置", "9x9 坐标、双方起始预备区与 a-h 八方位插孔配置" },
                { "ch2", "第 2 章", "完整回合行动", "插入 Prong、单格步进、多段连跳、部署预备区 Pod 与解救" },
                { "ch3", "第 3 章", "堆叠、组合走法与附录 A 特殊规则", "同格多 Pod 逗号分进、A.1 环路跳回、无边界环绕与 SuperProng" },
                { "ch4", "第 4 章", "胜利条件与 4P 基地淘汰", "3 基地与 1 基地占领胜利、4P 淘汰及 30／60 手无进展和棋" },
                { "ch5", "第 5 章", "13 种标准变体对照表", "棋盘尺寸、Pod／Prong 配额、特殊规则与计分对局资格" },
                { "ch6", "第 6 章", "官方棋谱记号与 OCTI-FEN 语法", "代数走法语法与 11／13 字段 OCTI-FEN 局面格式" },
                { "ch7", "第 7 章", "双组别 Elo 等级分与竞技场规则", "纯人类排名赛（第 1 组）与开放机器人竞技场（第 2 组）规则" }
            },
            "OCTI 官方图解规则书",
            "涵盖完整多变体对局规则、战术机制、附录 A 特殊规则、官方棋谱记号／OCTI-FEN 语法与双组别 Elo 竞技规则。",
            "章节导览：",
            "第 1 章 &mdash; 棋盘几何、Pod、Prong 与开局设置",
            "OCTI 是一款完全信息、零运气的向量构筑抽象策略棋盘游戏。每颗 Pod 进场时都是空白状态（没有任何移动方向），随着玩家每回合插入一支 Prong，逐步获得各方向的机动能力。",
            "Pod（八角形主棋子，周围有 8 个方向插孔）",
            "Prong（插入 Pod 插孔以赋予移动方向的方向插销）",
            "SuperProng（金色特殊 Prong，每支可在常规行动前提供一次直线奖励移动）",
            "<b>9&times;9 标准棋盘：</b>各列（纵列）<code>x = 1..9</code> 由西向东排列（从左到右），各行（横行）<code>y = 1..9</code> 由南向北排列（从下到上）；每个棋格以两位数 <code>xy</code> 表示（例如 <code>53</code> 代表第 5 列、第 3 行）。",
            "<b>双人对局初始资源（<code>2P_3B</code> 与 <code>2P_1B</code>）：</b>玩家 A 开局在己方 OCTI 大本营基地 <code>33</code>、<code>53</code>、<code>73</code> 各放 <b>1 颗 Pod</b>（共 <b>3 颗 Pod</b>），玩家 B 开局在己方 OCTI 大本营基地 <code>37</code>、<code>57</code>、<code>77</code> 各放 <b>1 颗 Pod</b>（共 <b>3 颗 Pod</b>）；每方另持有 <b>4 颗预备区 Pod</b>（每方共 7 颗 Pod）与 <b>25 支 Prong</b>（在 <code>_SP</code> 变体中为 <b>24 支普通 Prong + 1 支金色 SuperProng</b>）。",
            "<b>8 方位罗盘 Prong 插孔（<code>a</code>&ndash;<code>h</code>）：</b>每颗八角形 Pod 周围都有 8 个插孔，由北方起顺时针依次为：<code>a</code>=北（<code>0,+1</code>）、<code>b</code>=东北（<code>+1,+1</code>）、<code>c</code>=东（<code>+1,0</code>）、<code>d</code>=东南（<code>+1,-1</code>）、<code>e</code>=南（<code>0,-1</code>）、<code>f</code>=西南（<code>-1,-1</code>）、<code>g</code>=西（<code>-1,0</code>）、<code>h</code>=西北（<code>-1,+1</code>）。",
            "图 1：标准 9x9 双人对局配置（2P_3B），双方起始基地分别为 33/53/73 与 37/57/77。",
            "图 2：2P_1B（单基地快棋）采用完全相同的开局配置，但只要攻入任 1 座敌方基地（55ab-57x）即可获胜。",
            "图 3：Pod A55acg 周围的 8 个方位插孔 a-h，以及从备用 Prong 中取一支插入空插孔（55acg+b）。",
            "第 2 章 &mdash; 完整回合行动",
            "玩家 A 先手，各方严格轮流行动（在 <code>4P</code> 中行动顺序按 <code>A &rarr; B &rarr; C &rarr; D</code> 轮转，并略过已淘汰座位）。严禁空过回合；轮到你时，必须执行<b>恰好一项</b>常规回合行动：",
            "<b>插入 Prong（<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>）：</b>从你的备用 Prong 中取 1 支，插入棋盘上任一颗己方 Pod 的空插孔（例如 <code>55acg+b</code>；金色 SuperProng 记为 <code>33+b!</code>）。Prong 一旦插入该 Pod 即永久固定——在任何变体中均严禁拔出或移到其他插孔。",
            "<b>单格步进（<code>xy&lt;cfg&gt;-x'y'</code>）：</b>将一颗 Pod 沿其任一已安装的 Prong 方向移动 1 格，进入空格或你自己的 Pod 堆叠（例如 <code>44ab-45</code>）。单格步进<b>绝不吃子</b>，也不可进入有其他玩家 Pod 的棋格（包括 <code>4P</code> 中的对角队友）。",
            "<b>双格跳跃与多段连跳（<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>）：</b>沿已安装的 Prong 方向越过相邻的有子棋格（友方或敌方皆可），跳 2 格落入其后方的空格。同一颗 Pod 可沿其任意已安装的 Prong 方向继续连跳（途中可转向），也可在任意一次合法跳跃后提前停下。",
            "<b>部署预备区 Pod（<code>xy</code>）或解救被俘 Pod（<code>xy*</code>）：</b>当你自己的大本营基地为空格或仅有你自己的 Pod 时，将 1 颗预备区 Pod 放到该基地上（或放到你已占领的敌方基地上）；或在 3 基地（<code>2P_3B*</code>）与 4 人（<code>4P*</code>）对局中，符合条件时将 1 颗你自己的被俘 Pod 解救（<code>xy*</code>）到你已占领的 OCTI 基地格上（解救不适用于单基地决胜变体与 <code>OFK</code>）。",
            "<b>选择性整叠吃子与缴获 Prong：</b>跳过有子棋格时，吃子是<b>可选</b>的：附加 <code>x</code>（如 <code>44ab-66x</code>）即可吃掉<b>被跳过堆叠中的所有 Pod</b>（整叠吃子——严禁只吃掉堆叠中的部分棋子），省略 <code>x</code>（如 <code>44ab-66</code>）则保留被跳过的堆叠。在常规的单一走法或组合走法中，被吃掉的堆叠在回合结束时移入各 Pod 原主人的 <code>capturedPods</code>（被俘 Pod）池，同时<b>其上附着的所有 Prong 全数转入吃子方的备用 Prong 存量</b>。友方／盟友 Pod 可自由跳越而不必吃子。",
            "图 4：Pod A44ab 向北步进 1 格（44ab-45），对照向东北越过 B55e 跳 2 格，并可选择整叠吃子（44ab-66x）。",
            "图 5：多段连跳 33abd-55-57x-75x：先越过友方 A44h，转向北方吃掉 B56e，再转向东南吃掉 B66c。",
            "图 7：在空的大本营基地 33 部署预备区 Pod，对照在 2P_3B* 与 4P* 中占领敌方（或 4P 中立）OCTI 基地格后解救己方被俘 Pod（53* 或 57*）。",
            "第 3 章 &mdash; 堆叠、组合走法与附录 A 特殊规则",
            "<b>自身 Pod 堆叠与多 Pod 组合走法（<code>,</code>）：</b>多颗属于你自己颜色的 Pod 可堆叠在同一格（<code>OFK</code> 禁止堆叠；在 <code>4P</code> 中，严禁与其他颜色——包括你的对角队友——堆叠在同一格）。凡是<b>回合开始时位于同一格</b>的己方 Pod，可任选其中几颗或全部在同一回合一起移动，或沿各自独立的步进／跳跃路径分进，以逗号连接（如 <code>44a-45,44beg-34</code>）。部署 Pod 与插入 Prong 绝不能与移动合并在同一回合。",
            "<b>整叠吃子与回合结束移除：</b>以 <code>x</code> 跳过有子格会吃掉<b>该格堆叠中的每一颗 Pod</b>。在常规单一走法与组合走法中，以 <code>x</code> 跳过的堆叠在同一回合内仍留在棋盘上，可供同叠其余 Pod 进行组合走法时作为跳板，并在回合结束时一并移除。同一颗 Pod 在一条连跳链中<b>绝不可两次跳过同一个棋格</b>。",
            "<b>附录 A.1 &mdash; 起点格腾空、环路跳回与同叠 Pod 不可互跳：</b>在整条跳跃链期间，跳跃中的 Pod <b>不得跳过自己的起点格 <code>S</code>，也不得跳过任何回合开始时与它同叠的 Pod</b>（因此无边界 <code>_ED</code> 环绕跳跃如 <code>44bcd-66-86-16-34-54</code> 若跳过留在 <code>44</code> 未动的 <code>A44a</code> 即属违规）。此外，跳跃中的 Pod 若要在多段连跳环路结束时<b>落回 <code>S</code></b>，<b>仅限回合开始时位于 <code>S</code> 的所有 Pod 均在本次走法中离开 <code>S</code></b> 时方为合法（如组合腾空走法 <code>44a-45,44beg-66x-64x-44</code> 因 <code>44</code> 已完全腾空而合法；反之单走 <code>44beg-66x-64x-44</code> 而让 <code>A44a</code> 留在 <code>44</code> 则属违规）。不吃子的环形多段连跳（<code>S</code> 完全腾空时的 <code>S &rarr; ... &rarr; S</code>）仍然合法，并计为纯移动的一手（无进展和棋计数 <code>+1</code>）。",
            "<b>Pod 解救适用范围与条件（<code>2P_3B*</code> 与 <code>4P*</code>）：</b>解救被俘 Pod（<code>xy*</code>）仅适用于 <b>3 基地（<code>2P_3B*</code>）与 4 人（<code>4P*</code>）</b>对局（绝不适用于单基地决胜变体或 <code>OFK</code>），每次只能解救 1 颗<b>你自己的</b>被俘 Pod（<code>capturedPods &gt; 0</code>），且目标 OCTI 基地格必须<b>已有至少一颗你自己的 Pod</b>（不可是空格或仅有队友 Pod 的棋格）。在 <code>2P_3B*</code> 中，解救到己方大本营基地还须同时占领至少一座敌方基地；直接解救到已占领的敌方基地则只需 <code>capturedPods &gt; 0</code>。在 <code>4P*</code> 中，解救到自己或队友的角落基地须由你本人占领一座中立（<code>53</code>／<code>57</code>）或敌方 OCTI 基地格（即使队友已被淘汰，在其角落基地执行 <code>xy*</code> 也只会解救你自己的 Pod，绝不会恢复已淘汰的队友）。",
            "<b>无边界环面连通（<code>_ED</code>）：</b>第 <code>1</code> 列与第 <code>9</code> 列相连、第 <code>1</code> 行与第 <code>9</code> 行相连（包括对角线方向），单格步进（<code>15g-95</code>）与双格跳跃（<code>97b-29x</code> 跳过 <code>B18e</code>）均可跨边环绕。",
            "<b>SuperProng 与奖励前置移动（<code>_SP</code>，<code>;</code>）：</b>金色 SuperProng（<code>!</code>）一旦插入即永久固定。在执行常规回合行动之前，<b>你在棋盘上每控制 1 支 SuperProng，即可执行 1 次奖励前置移动</b>（缴获敌方 SuperProng 后，<code>2P_SP</code> 最多 <b>2</b> 次、<code>4P_SP</code> 最多 <b>4</b> 次），以分号分隔（如 <code>44ab!-66x;66ab!-67</code>）。每次前置移动只能移动 1 颗 Pod，严格沿本回合尚未发动过奖励前置移动的某支 SuperProng 插孔方向按<b>直线</b>移动（单格步进或共线连跳，禁止转向）。在 SuperProng 奖励前置移动中以 <code>x</code> 跳过的堆叠会在该次前置移动后<b>立即移除</b>，随后才开始下一次 SuperProng 前置移动或常规行动！在奖励前置移动中移动过的 Pod 在常规回合阶段仍可使用其<b>任意</b> Prong，且<b>胜负仅在完整回合结束时判定</b>。",
            "图 6：堆叠 [A44a, A44beg] 通过 44a-45,44beg-34 分进，或通过 44a-45,44beg-66x-64x-44 完全腾空 44 使 A44beg 合法落回已腾空的 44（附录 A.1）。",
            "图 8：无边界（_ED）跨边步进 15g-95 与跨边界对角跳跃吃子 97b-29x。",
            "图 9：金色 SuperProng 直线奖励前置移动 44ab!-66x;（棋盘上每控制 1 支 SuperProng 可执行 1 次奖励移动）在常规步进 66ab!-67 之前立即移除被吃掉的 B55e!，并于完整回合结束时判定胜负。",
            "第 4 章 &mdash; 胜利、淘汰、困毙与无进展和棋条件",
            "<b><code>2P_3B</code> 系列（3 基地标准赛）：</b>在你的完整回合结束时，若己方 Pod 同时占领<b>全部 3 座</b>敌方大本营基地（A 的目标为 <code>37</code>、<code>57</code>、<code>77</code>；B 的目标为 <code>33</code>、<code>53</code>、<code>73</code>），即获胜。",
            "<b><code>2P_1B</code> 系列（单基地快棋）：</b>在你的完整回合结束时，只要己方 Pod 占领 3 座敌方大本营基地中的<b>任 1 座</b>，即获胜。",
            "<b><code>OFK</code>（OCTI for Kids，6&times;7）：</b>在你的完整回合结束时，只要己方 Pod 占领 4 座敌方大本营基地中的<b>任 1 座</b>（A 的目标为 <code>26, 36, 46, 56</code>；B 的目标为 <code>22, 32, 42, 52</code>），即获胜。",
            "<b><code>4P</code> 联队系列（A&amp;C 队对 B&amp;D 队）：</b>在完整回合结束时，若同盟 Pod 同时占领<b>敌队两座角落基地</b>（A&amp;C 队为 <code>37</code> 与 <code>73</code>；B&amp;D 队为 <code>33</code> 与 <code>77</code>），对角队友即共同获胜。",
            "<b>4 人模式基地失守永久淘汰与队友基地安全规则：</b>在 <code>4P</code> 中，若<b>敌方</b> Pod 在完整回合结束时停在你的角落大本营基地上（或你的所有 Pod 均被吃掉且剩余预备区 Pod 为 0），你将在本局剩余时间内被<b>永久淘汰</b>：你场上残存的所有 Pod 会悉数扫入你的 <code>capturedPods</code>（被俘 Pod）池，且<b>这些被扫离棋盘的 Pod 上的所有 Prong（普通 Prong 与 SuperProng）全数转交给占领基地的敌方玩家</b>。回合轮转时会略过已淘汰座位，由尚存队友继续作战。相反，<b>步进或跳跃进入对角队友空着（或仅有你自己的 Pod 驻守）的起始基地完全合法且没有任何负面后果——绝不会将你的队友淘汰出局</b>（若你在棋盘上没有任何 Pod 时队友暂时停留在你的起始基地上，则只会略过你的回合，直到队友腾出该基地为止）。",
            "<b>全灭／困毙（无子可动）：</b>由于不得移动已安装的 Prong，当玩家预备区无 Pod 也无备用 Prong，且场上 Pod 完全被封死时，即<b>没有任何合法走法</b>。在 <code>2P</code>／<code>OFK</code> 中若下一位玩家无合法走法（或在 <code>4P</code> 中敌方两个座位均无合法走法），刚完成行动的玩家／队伍立即获胜（在 <code>4P</code> 中，若困毙玩家的队友仍有合法走法，则只略过该玩家的回合）。计时对局也会因超时判负或认输而结束。",
            "<b>通用无进展和棋（<code>NO_PROGRESS_DRAW</code>，<code>noProgressPlies</code>）：</b>在任何 <code>2P*</code> 或 <code>OFK</code> 变体中<b>连续 <code>30</code> 手（每位玩家各 <code>15</code> 个完整回合）</b>——或在任何 <code>4P*</code> 变体中<b>连续 <code>60</code> 手（<code>15</code> 个完整轮次）</b>——未发生任何<b>进展事件</b>（吃掉 Pod <code>x</code>［含 <code>_SP</code> 奖励前置移动］、部署预备区 Pod <code>xy</code> 或解救 Pod <code>xy*</code>、插入 Prong／SuperProng <code>+d</code>／<code>+d!</code>，或 <code>4P</code> 玩家永久淘汰）时，对局自动判为<b>和棋（<code>NO_PROGRESS_DRAW</code>）</b>。此条件于回合结束时排在基地占领胜利、全灭对手与无合法走法（困毙）判定<b>之后</b>检查（因此在第 <code>30</code>／<code>60</code> 手达成胜利条件仍算获胜）。",
            "图 10：4 人联队模式（4P）：对角盟友 A(33)&C(77) 对 B(37)&D(73)、同色堆叠 [A55ab, A55f]、队友 Pod C65cf、安全进入队友基地，以及中立格 53／57。",
            "图 11：6x7 棋盘上的 OCTI for Kids（OFK）：每方 4 座基地、0 颗预备区 Pod、Prong 永久固定、禁止堆叠，占领 1 座基地即获胜（34a-36x）。",
            "图 12：2P_3B 胜利局面（status = O），玩家 A 同时占领全部 3 座敌方基地（A37c, A57a, A77g）。",
            "第 5 章 &mdash; 13 种标准变体对照表",
            "所有平台（网页端、桌面端、Android、棋谱库与机器人 SDK）中的每场对局均使用下列 <b>13 种标准变体代码</b>之一：",
            "代码", "棋盘", "人数", "Pod（起始+预备）", "Prong（普通／SP）", "特殊规则与胜利目标", "计分资格",
            "计分", "休闲",
            "标准规则；同时占领全部 3 座敌方基地",
            "SuperProng <code>!</code> + <code>;</code> 直线奖励前置移动（&le;2）；3 座基地",
            "无边界环面连通；3 座基地",
            "SuperProng（&le;2 次前置移动）+ 无边界；3 座基地",
            "决胜快棋；占领任 1 座敌方基地",
            "SuperProng（&le;2 次前置移动）+ 单基地决胜",
            "无边界 + 单基地决胜",
            "SuperProng（&le;2 次前置移动）+ 无边界 + 单基地",
            "中立格 <code>53/57</code>、基地失守永久淘汰；占领 2 座敌方角落基地",
            "4P 联队 + SuperProng 直线奖励前置移动（&le;4）",
            "4P 联队 + 无边界环面连通",
            "4P 联队 + SuperProng（&le;4 次前置移动）+ 无边界",
            "每方 4 座基地、0 预备区、禁止堆叠与组合走法；占领 1 座基地即获胜",
            "第 6 章 &mdash; 官方棋谱记号与 OCTI-FEN 规范",
            "走法记号语法",
            "语法", "行动说明", "示例",
            "将预备区 Pod 部署到 OCTI 基地格",
            "在已占领的 OCTI 基地格解救己方被俘 Pod（仅限 <code>2P_3B*</code> 与 <code>4P*</code>）",
            "在插孔 <code>d &isin; {a..h}</code> 插入普通 Prong",
            "在插孔 <code>d</code> 插入金色 SuperProng",
            "不吃子的单格步进",
            "单段或多段连跳链（<code>x</code> = 选择性整叠吃子）",
            "同一起点格多颗 Pod 分进的组合走法",
            "1&ndash;4 次直线 SuperProng 奖励前置移动（场上每支 SP 限 1 次；<code>x</code> 立即移除）+ 常规回合行动",
            "OCTI-FEN 局面字符串格式",
            "棋盘状态以冒号分隔序列化为 <b>OCTI-FEN</b> 字符串（<code>2</code>／<code>OFK</code> 为 11 个字段；<code>4</code> 为 13 个字段）：",
            "<code>mode</code>：<code>2</code>、<code>4</code> 或 <code>OFK</code> &bull; <code>bases</code>：<code>3</code> 或 <code>1</code> &bull; <code>super</code> / <code>edgeless</code>：<code>0</code> 或 <code>1</code>。",
            "<code>status</code>：<code>N</code>（未开始）、<code>P</code>（进行中）、<code>O</code>（对局结束）、<code>R</code>（认输）。",
            "<code>turn</code> / <code>winner</code>：<code>A</code>、<code>B</code>、<code>C</code>、<code>D</code> 或 <code>-</code> &bull; <code>elimMask</code>：4P 淘汰位掩码（<code>A=1, B=2, C=4, D=8</code>）。",
            "<code>resP</code>：<code>freePods,capturedPods,freeProngs,freeSuperProngs</code>（例如 <code>4,0,25,0</code>）。",
            "<code>squares</code>：按行优先顺序以逗号分隔的棋盘 Pod <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code>（例如 <code>A44ab!,B55e!</code>），空盘为 <code>-</code>。",
            "<code>noProgressPlies</code>（可选的第 12／14 个末尾字段）：连续无进展手数（<code>0..1000</code>，省略时默认为 <code>0</code>；标准 <code>toPositionString()</code> 输出 11／13 个字段的字符串）。",
            "第 7 章 &mdash; 双组别 Elo 等级分与竞技场规则",
            "官方等级分起始为 <b>1200 Elo</b>，分设三项独立的 1v1 变体排名赛：<b><code>2P_3B</code></b>、<b><code>2P_1B</code></b> 与 <b><code>OFK</code></b>。所有 4 人（<code>4P*</code>）、SuperProng（<code>*_SP</code>）、无边界（<code>*_ED</code>）、混合变体（<code>*_SPED</code>）及离线对局均为休闲（不计分）对局。",
            "<b>第 1 组 &mdash; 纯人类排名赛（排行榜与玩家名录）：</b><br>仅计入人类对战人类的在线计分对局（标准系数 <code>K = 32</code>）。纯人类等级分与机器人对局完全隔离，不受影响。",
            "<b>第 2 组 &mdash; 开放机器人与人类竞技场（排行榜与机器人名录）：</b><br>计入至少有一方为已注册 AI 机器人的在线计分对局（人类对机器人、机器人对机器人），并设有三项防刷分保护机制：<ul><li><b>人类对机器人对局：</b>采用标准 <code>K = 32</code>，计分场次不限（包括对战你自己的机器人）。</li><li><b>机器人对机器人 K 系数：</b>不同拥有者之间的对局采用标准 <code>K = 32</code>；同一拥有者的内部自测采用折减系数 <code>K = 16</code>。</li><li><b>机器人配对场次上限与提前认输保护：</b>任意一对机器人组合（不分先后手）在<b>滚动 24 小时窗口内最多 4 局计分</b>（第 5 局起自动转为休闲对局；对战不同对手机器人则无每日上限），且未满 <b>8 手</b>即认输的对局 Elo 变动记为 0。</li></ul>"
        },
        /* 3. French (fr) */
        {
            "fr",
            {
                { "ch1", "Chapitre 1", "Géométrie du plateau, Pod, Prong et mise en place", "Coordonnées 9x9, réserves initiales et 8 cavités directionnelles a-h" },
                { "ch2", "Chapitre 2", "Les actions du tour", "Insertion de Prong, pas d’une case, chaînes de sauts, pose et libération" },
                { "ch3", "Chapitre 3", "Piles, coups composés et annexe A", "Coups composés avec virgule, boucles A.1, sans bords et SuperProng" },
                { "ch4", "Chapitre 4", "Victoire, élimination, blocage et nulle", "Occupation de 3 bases ou d’1 base, élimination en 4P, blocage et nulle après 30/60 coups" },
                { "ch5", "Chapitre 5", "Tableau des 13 variantes canoniques", "Grilles, dotations en Pod et Prong, options et éligibilité Elo" },
                { "ch6", "Chapitre 6", "Notation des coups et grammaire OCTI-FEN", "Grammaire algébrique officielle et format OCTI-FEN à 11/13 champs" },
                { "ch7", "Chapitre 7", "Elo à deux bassins et arène", "Bassin 1 réservé aux humains, bassin 2 : arène ouverte aux robots et aux humains" }
            },
            "Livre officiel illustré des règles OCTI",
            "Règles complètes de toutes les variantes, mécanismes tactiques, cas particuliers de l’annexe A, grammaire OCTI-FEN et règlement Elo à deux bassins.",
            "Chapitres :",
            "Chapitre 1 &mdash; Géométrie du plateau, Pod, Prong et mise en place",
            "OCTI est un jeu de stratégie abstrait sans hasard et à information complète, fondé sur la construction de directions de déplacement. Chaque Pod entre vierge sur le plateau, sans aucune direction de déplacement, et gagne en mobilité à mesure que les joueurs y insèrent des Prong, un par tour.",
            "Pod (pièce octogonale principale dotée de 8 cavités directionnelles)",
            "Prong (cheville que l’on enfiche dans une cavité d’un Pod pour lui ouvrir une direction de déplacement)",
            "SuperProng (Prong doré spécial qui permet, avant l’action normale du tour, un déplacement bonus en ligne droite)",
            "<b>Grille standard 9&times;9 :</b> les colonnes <code>x = 1..9</code> vont d’ouest en est (de gauche à droite) ; les rangées <code>y = 1..9</code> vont du sud au nord (de bas en haut). Chaque case s’écrit avec deux chiffres <code>xy</code> (par exemple, <code>53</code> désigne la colonne 5, rangée 3).",
            "<b>Matériel de départ à 2 joueurs (<code>2P_3B</code> et <code>2P_1B</code>) :</b> le joueur A commence avec 3 Pod sur ses bases OCTI de départ <code>33</code>, <code>53</code> et <code>73</code> ; le joueur B, avec 3 Pod sur <code>37</code>, <code>57</code> et <code>77</code>. Chaque camp dispose en outre de 4 Pod de réserve (7 Pod au total par joueur) et de 25 Prong (ou de 24 Prong normaux + 1 SuperProng doré dans les variantes <code>_SP</code>).",
            "<b>8 cavités directionnelles (<code>a</code>&ndash;<code>h</code>) :</b> attribuées dans le sens horaire à partir du nord : <code>a</code>=N, <code>b</code>=NE, <code>c</code>=E, <code>d</code>=SE, <code>e</code>=S, <code>f</code>=SO, <code>g</code>=O, <code>h</code>=NO.",
            "Figure 1 : position de départ standard 9x9 à 2 joueurs (2P_3B).",
            "Figure 2 : 2P_1B utilise la même position de départ, mais il suffit d’infiltrer une seule base adverse pour gagner.",
            "Figure 3 : les 8 cavités a-h autour du Pod A55acg et l’insertion 55acg+b.",
            "Chapitre 2 &mdash; Les actions du tour",
            "Le joueur A commence et les tours alternent strictement (en 4P, l’ordre est A &rarr; B &rarr; C &rarr; D, en sautant les places éliminées). Il est interdit de passer : à votre tour, vous devez jouer exactement une action normale :",
            "<b>Insérer un Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>) :</b> prenez 1 Prong dans votre réserve libre et insérez-le dans une cavité vide de l’un de vos Pod sur le plateau (par ex. <code>55acg+b</code> ; ou <code>33+b!</code> pour un SuperProng doré). Une fois inséré, un Prong reste définitivement sur ce Pod.",
            "<b>Pas d’une case (<code>xy&lt;cfg&gt;-x'y'</code>) :</b> déplacez un Pod d’une case, dans la direction de l’un de ses Prong installés, vers une case vide ou sur une pile de vos propres Pod (par ex. <code>44ab-45</code>). Un pas d’une case <b>ne prend jamais</b> et ne peut pas entrer sur une case occupée par le Pod d’un autre joueur (y compris votre partenaire en diagonale en <code>4P</code>).",
            "<b>Saut de 2 cases et chaîne de sauts (<code>xy&lt;cfg&gt;-x1y1[x]-...</code>) :</b> sautez de 2 cases dans la direction de l’un des Prong du Pod, par-dessus une case adjacente occupée (amie ou adverse), pour atterrir sur la case vide juste derrière. Vous pouvez enchaîner d’autres sauts avec ce même Pod, dans n’importe laquelle de ses directions, et vous arrêter après n’importe quel saut légal.",
            "<b>Poser un Pod de réserve (<code>xy</code>) ou libérer un Pod pris (<code>xy*</code>) :</b> posez 1 Pod de réserve sur votre propre base de départ lorsqu’elle est vide ou occupée uniquement par vos propres Pod (ou sur une base adverse que vous occupez déjà), ou, dans les parties à 3 bases (<code>2P_3B*</code>) et à 4 joueurs (<code>4P*</code>), libérez 1 de vos propres Pod pris (<code>xy*</code>) sur une case OCTI que vous occupez déjà lorsque les conditions sont remplies (la libération n’existe ni dans les variantes à 1 base en mort subite ni en <code>OFK</code>).",
            "<b>Prise facultative de la pile entière et butin de Prong :</b> lorsque vous sautez par-dessus une case occupée, la prise est facultative : ajoutez <code>x</code> (par ex. <code>44ab-66x</code>) pour prendre tous les Pod de la pile sautée (une prise partielle n’est jamais autorisée), ou omettez <code>x</code> (par ex. <code>44ab-66</code>) pour laisser la pile intacte. Lors d’un coup normal, simple ou composé, les piles prises sont retirées en fin de tour vers le stock <code>capturedPods</code> de leur propriétaire, et tous leurs Prong passent dans la réserve de Prong libres du joueur qui a effectué la prise.",
            "Figure 4 : pas d’une case (44ab-45) ou saut avec prise de la pile entière (44ab-66x).",
            "Figure 5 : rafle 33abd-55-57x-75x avec changements de direction et prises.",
            "Figure 7 : pose d’un Pod de réserve sur 33 et libération (53* ou 57*) en 2P_3B* et en 4P*.",
            "Chapitre 3 &mdash; Piles, coups composés et cas particuliers de l’annexe A",
            "<b>Piles de vos propres Pod et coups composés à plusieurs Pod (<code>,</code>) :</b> plusieurs Pod de votre propre couleur peuvent s’empiler sur la même case (interdit en <code>OFK</code> ; en <code>4P</code>, s’empiler avec une autre couleur &mdash; y compris votre partenaire en diagonale &mdash; n’est jamais autorisé). N’importe quel groupe de vos propres Pod qui <b>commencent le tour sur la même case</b> peut se déplacer ensemble ou se séparer sur des trajets distincts (pas ou sauts) au cours d’un même tour ; les déplacements sont séparés par des virgules (par ex. <code>44a-45,44beg-34</code>). La pose d’un Pod et l’insertion d’un Prong ne se combinent jamais avec un déplacement.",
            "<b>Prise de la pile entière et retrait en fin de tour :</b> les piles prises avec <code>x</code> restent sur le plateau comme points d’appui pour les autres Pod du coup composé et sont retirées en fin de tour ; un même Pod ne peut jamais sauter deux fois par-dessus la même case au cours d’une chaîne de sauts.",
            "<b>Annexe A.1 &mdash; Case de départ libérée et sauts en boucle :</b> pendant sa chaîne de sauts, un Pod ne peut sauter ni par-dessus sa case de départ <code>S</code> ni par-dessus un Pod qui était dans sa pile au début du tour (<code>44bcd-66-86-16-34-54</code> par-dessus <code>A44a</code> est illégal). Il ne peut revenir sur <code>S</code> que si tous les Pod qui se trouvaient sur <code>S</code> au début du tour la quittent pendant le coup (<code>44a-45,44beg-66x-64x-44</code> est légal). Une boucle sans prise compte comme un coup de simple déplacement pour la nulle par absence de progrès.",
            "<b>Portée de la libération (<code>2P_3B*</code> et <code>4P*</code>) :</b> <code>xy*</code> libère toujours 1 de vos propres Pod pris et exige qu’au moins un de vos propres Pod occupe déjà la case OCTI visée (jamais une case vide ou occupée seulement par votre partenaire). En <code>2P_3B*</code>, libérer sur votre propre base de départ exige en plus d’occuper au moins une base adverse ; en <code>4P*</code>, libérer sur votre base en coin ou celle de votre partenaire exige d’occuper vous-même une case OCTI neutre (<code>53</code>/<code>57</code>) ou adverse.",
            "<b>Bouclage du plateau sans bords (<code>_ED</code>) :</b> la colonne 1 se raccorde à la colonne 9, et la rangée 1 à la rangée 9 (diagonales comprises), pour les pas comme pour les sauts (par ex. <code>15g-95</code>, <code>97b-29x</code>).",
            "<b>SuperProng et préludes bonus (<code>_SP</code>, <code>;</code>) :</b> avant l’action normale du tour, vous pouvez jouer 1 déplacement bonus en prélude par SuperProng que vous contrôlez sur le plateau (par ex. <code>44ab!-66x;66ab!-67</code>) : un pas ou une chaîne de sauts alignés, en ligne droite, dans la direction d’une cavité à SuperProng distincte. Les piles prises pendant un prélude sont retirées immédiatement, et la victoire n’est vérifiée qu’à la fin du tour complet.",
            "Figure 6 : séparation de pile 44a-45,44beg-34 et boucle légale 44a-45,44beg-66x-64x-44 (annexe A.1).",
            "Figure 8 : pas avec bouclage sans bords 15g-95 et saut diagonal avec prise 97b-29x.",
            "Figure 9 : prélude bonus de SuperProng 44ab!-66x; suivi du coup normal 66ab!-67.",
            "Chapitre 4 &mdash; Victoire, élimination, blocage et nulle par absence de progrès",
            "<b>Famille <code>2P_3B</code> :</b> vous gagnez à la fin de votre tour complet si vos Pod occupent simultanément les 3 bases de départ adverses.",
            "<b>Famille <code>2P_1B</code> :</b> vous gagnez à la fin de votre tour complet si l’un de vos Pod occupe n’importe laquelle des 3 bases de départ adverses.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7) :</b> vous gagnez à la fin de votre tour complet si l’un de vos Pod occupe l’une des 4 bases de départ adverses.",
            "<b>Famille <code>4P</code> (équipe A et C contre équipe B et D) :</b> les partenaires gagnent ensemble à la fin d’un tour complet lorsque des Pod alliés occupent simultanément les 2 bases adverses en coin.",
            "<b>Élimination définitive en 4P et base du partenaire sûre :</b> en <code>4P</code>, si un Pod <b>adverse</b> termine son tour complet sur votre base de départ en coin (ou si tous vos Pod sont pris et qu’il ne vous reste aucun Pod en réserve), vous êtes <b>éliminé définitivement</b> pour le reste de la partie : tous vos Pod restants rejoignent votre stock <code>capturedPods</code>, et <b>tous les Prong (normaux et SuperProng) de ces Pod retirés passent au joueur adverse qui a occupé la base</b>. Les places éliminées sont sautées dans l’ordre de jeu tandis que le partenaire survivant continue de jouer. À l’inverse, <b>entrer par un pas ou un saut sur la base de départ vide de votre partenaire en diagonale (ou déjà occupée par vos propres Pod) est légal et sans aucune conséquence &mdash; cela n’élimine jamais votre partenaire</b> (si votre partenaire occupe temporairement votre base de départ alors que vous n’avez aucun Pod sur le plateau, votre tour est simplement sauté jusqu’à ce qu’il la libère).",
            "<b>Anéantissement total / blocage :</b> un joueur qui n’a plus ni Pod ni Prong en réserve et dont les Pod sont bloqués n’a aucun coup légal. Si le joueur suivant en <code>2P</code> / <code>OFK</code> (ou les deux places adverses en <code>4P</code>) n’a aucun coup légal, le joueur ou l’équipe qui vient de jouer gagne immédiatement ; en <code>4P</code>, un joueur bloqué dont le partenaire a encore un coup légal voit simplement son tour sauté.",
            "<b>Nulle par absence de progrès (<code>NO_PROGRESS_DRAW</code>) :</b> nulle automatique après 30 coups consécutifs en <code>2P*</code>/<code>OFK</code> (60 en <code>4P*</code>) sans prise, pose ni libération de Pod, insertion de Prong ni élimination en 4P. La règle est évaluée après les conditions de victoire.",
            "Figure 10 : 4 joueurs en équipes (4P) : alliés en diagonale A(33) et C(77) contre B(37) et D(73), pile de même couleur [A55ab, A55f], Pod du partenaire C65cf, entrée sûre sur la base du partenaire et cases neutres 53/57.",
            "Figure 11 : OCTI for Kids (OFK) sur grille 6x7 (34a-36x).",
            "Figure 12 : victoire en 2P_3B par occupation des 3 bases adverses (A37c, A57a, A77g).",
            "Chapitre 5 &mdash; Tableau de référence des 13 variantes canoniques",
            "Toutes les plateformes utilisent les <b>13 codes de variantes</b> suivants :",
            "Code", "Grille", "Joueurs", "Pod (départ+rés.)", "Prong (N/SP)", "Options et objectif", "Classée",
            "Oui", "Amicale",
            "Règles standard ; occuper les 3 bases adverses",
            "SuperProng <code>!</code> + préludes bonus en ligne droite <code>;</code> (&le;2) ; 3 bases",
            "Bouclage torique sans bords ; 3 bases",
            "SuperProng (&le;2 préludes) + sans bords ; 3 bases",
            "Blitz en mort subite ; occuper 1 base adverse",
            "SuperProng (&le;2 préludes) + mort subite à 1 base",
            "Sans bords + mort subite à 1 base",
            "SuperProng (&le;2 préludes) + sans bords + 1 base",
            "Bases neutres <code>53/57</code>, élimination définitive ; 2 bases adverses",
            "4P en équipes + préludes bonus de SuperProng en ligne droite (&le;4)",
            "4P en équipes + bouclage torique sans bords",
            "4P en équipes + SuperProng (&le;4 préludes) + sans bords",
            "OCTI for Kids : 4 bases par camp, aucune réserve, ni piles ni coups composés ; 1 base",
            "Chapitre 6 &mdash; Notation officielle des coups et OCTI-FEN",
            "Grammaire de notation des coups",
            "Syntaxe", "Action", "Exemple",
            "Poser un Pod de réserve libre sur une case OCTI",
            "Libérer un de ses Pod pris sur une case OCTI occupée (<code>2P_3B*</code> et <code>4P*</code> uniquement)",
            "Insérer un Prong normal dans la cavité <code>d &isin; {a..h}</code>",
            "Insérer un SuperProng doré dans la cavité <code>d</code>",
            "Pas d’une case, sans prise",
            "Saut simple ou chaîne de sauts (<code>x</code> = prise facultative de la pile entière)",
            "Coup composé séparant des Pod depuis la même case d’origine",
            "1 à 4 préludes bonus de SuperProng en ligne droite (retrait immédiat des prises <code>x</code>) + action normale",
            "Format de position OCTI-FEN",
            "Les positions sont sérialisées en 11 ou 13 champs séparés par des deux-points (plus <code>noProgressPlies</code> optionnel) :",
            "<code>mode</code> : <code>2</code>, <code>4</code> ou <code>OFK</code> &bull; <code>bases</code> : <code>3</code> ou <code>1</code> &bull; <code>super</code> / <code>edgeless</code> : <code>0</code> ou <code>1</code>.",
            "<code>status</code> : <code>N</code> (non commencée), <code>P</code> (en cours), <code>O</code> (terminée), <code>R</code> (abandonnée).",
            "<code>turn</code> / <code>winner</code> : <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> ou <code>-</code> &bull; <code>elimMask</code> : masque d’élimination 4P (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code> : <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (ex. <code>4,0,25,0</code>).",
            "<code>squares</code> : liste de Pod séparés par des virgules <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (ex. <code>A44ab!,B55e!</code>), ou <code>-</code> si vide.",
            "<code>noProgressPlies</code> (12e/14e champ optionnel) : nombre de coups consécutifs sans progrès (<code>0..1000</code>, défaut <code>0</code> ; <code>toPositionString()</code> émet les 11/13 champs canoniques).",
            "Chapitre 7 &mdash; Elo à deux bassins et règles de l’arène compétitive",
            "Les classements Elo officiels démarrent à <b>1200 Elo</b> et sont tenus séparément pour trois variantes en 1 contre 1 : <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b> et <b><code>OFK</code></b>. Toutes les parties à 4 joueurs, avec SuperProng, sans bords, en variante combinée et hors ligne sont amicales.",
            "<b>Bassin 1 &mdash; Classements entre humains :</b><br>Ne prend en compte que les parties classées en ligne entre humains (<code>K = 32</code>), totalement isolées des parties contre des robots.",
            "<b>Bassin 2 &mdash; Arène ouverte aux robots et aux humains :</b><br>Prend en compte les parties classées en ligne impliquant au moins un robot enregistré, avec trois garde-fous :<ul><li><b>Humain contre robot :</b> parties classées illimitées à <code>K = 32</code>.</li><li><b>Facteurs K entre robots :</b> <code>K = 32</code> entre propriétaires différents ; <code>K = 16</code> pour un même propriétaire.</li><li><b>Plafond par paire et abandon précoce :</b> au maximum <b>4 parties classées par période glissante de 24 heures</b> par paire de robots (au-delà, elles deviennent amicales), et aucune variation Elo en cas d’abandon avant <b>8 coups</b>.</li></ul>"
        },
        /* 4. Romanian (ro) */
        {
            "ro",
            {
                { "ch1", "Capitolul 1", "Geometria tablei, Pod, Prong și așezarea inițială", "Coordonate 9x9, rezerve inițiale și 8 orificii direcționale a-h" },
                { "ch2", "Capitolul 2", "Acțiunile unei ture", "Inserare de Prong, pas de 1 câmp, salturi în lanț, rezervă și eliberare" },
                { "ch3", "Capitolul 3", "Stivuire, mutări compuse și Anexa A", "Mutări multi-Pod cu virgulă, bucle A.1, Fără margini și SuperProng" },
                { "ch4", "Capitolul 4", "Victorie și eliminare 4P", "Ocuparea a 3 baze vs 1 bază, eliminare 4P și remiză la 30/60 de mutări" },
                { "ch5", "Capitolul 5", "Tabelul celor 13 variante canonice", "Dimensiuni, resurse Pod/Prong, modificatori și eligibilitate Elo" },
                { "ch6", "Capitolul 6", "Notația mutărilor și gramatica OCTI-FEN", "Gramatica algebrică oficială și formatul OCTI-FEN cu 11/13 elemente" },
                { "ch7", "Capitolul 7", "Regulile Elo pe două categorii și Arena", "Categoria 1 (doar oameni) vs Categoria 2 (arenă deschisă pentru roboți și oameni)" }
            },
            "Regulamentul oficial ilustrat OCTI",
            "Reguli complete pentru toate variantele, mecanici tactice, cazurile speciale din Anexa A, specificația OCTI-FEN și regulamentul Elo pe două categorii.",
            "Capitole:",
            "Capitolul 1 &mdash; Geometria tablei, Pod, Prong și așezarea inițială",
            "OCTI este un joc determinist de strategie abstractă cu informație completă, bazat pe construirea direcțiilor de deplasare. Fiecare Pod intră gol pe tablă, fără nicio direcție, și capătă mobilitate pe măsură ce jucătorii inserează piese Prong, câte una pe tură.",
            "Pod (piesă octogonală principală cu 8 orificii direcționale pe margine)",
            "Prong (pin direcțional inserat într-un orificiu al unei piese Pod pentru a-i deschide o direcție de deplasare)",
            "SuperProng (Prong auriu special care oferă o deplasare bonus preliminară în linie dreaptă înaintea acțiunii obișnuite a turei)",
            "<b>Grilă standard 9&times;9:</b> Coloanele <code>x = 1..9</code> merg de la Vest la Est; rândurile <code>y = 1..9</code> merg de la Sud la Nord. Fiecare câmp se scrie cu două cifre <code>xy</code> (de ex., <code>53</code> înseamnă coloana 5, rândul 3).",
            "<b>Resurse inițiale pentru 2 jucători (<code>2P_3B</code> și <code>2P_1B</code>):</b> Jucătorul A începe cu 3 piese Pod pe bazele proprii <code>33</code>, <code>53</code>, <code>73</code>; Jucătorul B cu 3 piese Pod pe <code>37</code>, <code>57</code>, <code>77</code>. Fiecare parte mai dispune de 4 piese Pod de rezervă (7 în total) și 25 de piese Prong (sau 24 de piese Prong normale + 1 SuperProng auriu în <code>_SP</code>).",
            "<b>Cele 8 orificii Prong, pe direcțiile busolei (<code>a</code>&ndash;<code>h</code>):</b> În sens orar de la Nord: <code>a</code>=N, <code>b</code>=NE, <code>c</code>=E, <code>d</code>=SE, <code>e</code>=S, <code>f</code>=SV, <code>g</code>=V, <code>h</code>=NV.",
            "Figura 1: Așezarea standard 9x9 pentru 2 jucători (2P_3B), cu bazele proprii 33/53/73 față de 37/57/77.",
            "Figura 2: 2P_1B (blitz la 1 bază) folosește aceeași așezare, dar ocuparea oricăreia dintre bazele adverse (55ab-57x) aduce victoria.",
            "Figura 3: Cele 8 orificii a-h ale piesei Pod A55acg și inserarea unui Prong (55acg+b).",
            "Capitolul 2 &mdash; Acțiunile unei ture",
            "Jucătorul A mută primul, iar turele alternează strict (în <code>4P</code>, ordinea se rotește <code>A &rarr; B &rarr; C &rarr; D</code>, sărind peste locurile eliminate). Renunțarea la mutare este interzisă; în tura ta trebuie să execuți exact o acțiune obișnuită:",
            "<b>Inserează un Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Ia 1 Prong din rezervă și inserează-l într-un orificiu liber al unui Pod propriu de pe tablă (de ex., <code>55acg+b</code> sau <code>33+b!</code>). Un Prong inserat rămâne definitiv pe acel Pod și nu poate fi mutat niciodată în alt orificiu.",
            "<b>Pas de 1 câmp (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Deplasează un Pod cu 1 câmp pe direcția unui Prong instalat, pe un câmp liber sau într-o stivă formată din propriile tale piese Pod (de ex., <code>44ab-45</code>). Un pas de 1 câmp <b>nu capturează niciodată</b> și nu poate intra pe un câmp ocupat de piesa Pod a altui jucător (inclusiv partenerul tău diagonal în <code>4P</code>).",
            "<b>Salt de 2 câmpuri și lanț de salturi (<code>xy&lt;cfg&gt;-x1y1[x]-...</code>):</b> Sari 2 câmpuri pe direcția unui Prong instalat, peste un câmp adiacent ocupat (propriu sau advers), pe câmpul liber aflat imediat dincolo. Poți înlănțui și alte salturi cu același Pod pe oricare dintre direcțiile sale Prong (schimbând direcția în mijlocul lanțului) și te poți opri după orice salt legal.",
            "<b>Plasează un Pod din rezervă (<code>xy</code>) sau eliberează un Pod capturat (<code>xy*</code>):</b> Plasează 1 Pod din rezervă pe baza ta proprie atunci când este liberă sau ocupată doar de propriile tale piese Pod (sau pe o bază adversă pe care o ocupi deja), ori &mdash; în partidele cu 3 baze (<code>2P_3B*</code>) și 4 jucători (<code>4P*</code>) &mdash; eliberează 1 Pod propriu capturat (<code>xy*</code>) pe un câmp OCTI pe care îl ocupi deja, atunci când condițiile sunt îndeplinite (eliberarea nu se aplică variantelor cu 1 bază sau <code>OFK</code>).",
            "<b>Captură opțională a întregii stive și preluarea pieselor Prong:</b> La săritura peste un câmp ocupat, captura este opțională: adaugă <code>x</code> (de ex., <code>44ab-66x</code>) pentru a captura toate piesele Pod din stiva sărită (captura parțială nu este permisă niciodată), sau omite <code>x</code> (de ex., <code>44ab-66</code>) pentru a lăsa stiva intactă. În mutările obișnuite simple sau compuse, stivele capturate sunt eliminate la finalul turei în rezerva <code>capturedPods</code> a proprietarului, iar toate piesele Prong atașate trec în rezerva liberă a jucătorului care a efectuat captura.",
            "Figura 4: Pas de 1 câmp (44ab-45) față de salt cu captură opțională a întregii stive (44ab-66x).",
            "Figura 5: Lanț de salturi 33abd-55-57x-75x cu pivotări și capturi.",
            "Figura 7: Plasare din rezervă pe 33 și eliberare (53* sau 57*) în 2P_3B* și 4P*.",
            "Capitolul 3 &mdash; Stivuire, mutări compuse și Anexa A",
            "<b>Stivuirea propriilor piese Pod și mutări compuse (<code>,</code>):</b> Mai multe piese Pod de propria ta culoare se pot stivui pe același câmp (interzis în <code>OFK</code>; în <code>4P</code>, stivuirea cu o altă culoare &mdash; inclusiv cu partenerul tău diagonal &mdash; nu este permisă niciodată). Orice grup de piese Pod proprii care <b>încep tura pe același câmp</b> se poate deplasa împreună sau se poate despărți pe trasee independente de pas/salt în aceeași tură, separate prin virgulă (de ex., <code>44a-45,44beg-34</code>). Plasarea unui Pod și inserarea unui Prong nu se combină niciodată cu deplasarea.",
            "<b>Captură integrală de stivă și eliminare la finalul turei:</b> În mutările obișnuite simple și compuse, stivele sărite cu <code>x</code> rămân fizic pe tablă ca puncte de sprijin pentru ceilalți colegi de stivă în aceeași tură și sunt eliminate împreună la finalul turei. Un singur Pod nu poate sări niciodată de două ori peste exact același câmp în același lanț.",
            "<b>Anexa A.1 &mdash; Golirea câmpului de plecare și buclele:</b> Un Pod nu poate sări peste câmpul său de plecare <code>S</code> și nici peste vreun Pod cu care a început tura în aceeași stivă (<code>44bcd-66-86-16-34-54</code> peste <code>A44a</code> este ilegal). Poate ateriza înapoi pe <code>S</code> doar dacă toate piesele Pod care au început tura pe <code>S</code> părăsesc câmpul <code>S</code> în timpul mutării (<code>44a-45,44beg-66x-64x-44</code> este legal). Buclele fără captură rămân legale și contează ca mutări de simplă deplasare (+1 către pragul de remiză prin lipsă de progres).",
            "<b>Condițiile eliberării (<code>2P_3B*</code> și <code>4P*</code>):</b> <code>xy*</code> eliberează 1 Pod propriu capturat și cere ca câmpul OCTI țintă să fie deja ocupat de cel puțin un Pod propriu (niciodată gol sau doar de partener). În <code>2P_3B*</code>, eliberarea pe o bază proprie necesită și ocuparea a cel puțin unei baze adverse; în <code>4P*</code>, eliberarea pe o bază din colț necesită ocuparea personală a unui câmp neutru (<code>53</code>/<code>57</code>) sau advers și nu readuce niciodată un partener eliminat.",
            "<b>Tablă toroidală fără margini (<code>_ED</code>):</b> Coloana 1 se leagă de coloana 9, iar rândul 1 de rândul 9, inclusiv pe diagonală, atât pentru pași (<code>15g-95</code>), cât și pentru salturi (<code>97b-29x</code>).",
            "<b>SuperProng și deplasări bonus preliminare (<code>_SP</code>, <code>;</code>):</b> Înainte de acțiunea obișnuită, poți efectua 1 deplasare bonus preliminară în linie dreaptă (pas sau lanț de salturi coliniare, fără pivotare) pentru fiecare SuperProng pe care îl controlezi pe tablă (până la 2 în <code>2P_SP</code> și 4 în <code>4P_SP</code>, de ex., <code>44ab!-66x;66ab!-67</code>), fiecare pe un orificiu SuperProng distinct. Stivele capturate cu <code>x</code> într-o deplasare bonus sunt eliminate imediat. Un Pod care s-a deplasat într-o deplasare bonus își poate folosi în continuare oricare dintre piesele Prong în faza obișnuită a turei, iar victoria se verifică doar la finalul întregii ture.",
            "Figura 6: Despărțirea stivei 44a-45,44beg-34 și bucla legală 44a-45,44beg-66x-64x-44 (Anexa A.1).",
            "Figura 8: Pas peste margine 15g-95 și salt diagonal cu captură 97b-29x pe tabla fără margini.",
            "Figura 9: Deplasare bonus SuperProng 44ab!-66x; urmată de pasul obișnuit 66ab!-67.",
            "Capitolul 4 &mdash; Victorie, eliminare 4P, blocaj și remiză prin lipsă de progres",
            "<b>Familia <code>2P_3B</code>:</b> Câștigi la finalizarea turei tale complete dacă piesele tale Pod ocupă simultan toate cele 3 baze adverse.",
            "<b>Familia <code>2P_1B</code>:</b> Câștigi la finalizarea turei tale complete atunci când un Pod propriu ocupă oricare dintre cele 3 baze adverse.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Câștigi la finalizarea turei tale complete atunci când un Pod propriu ocupă oricare dintre cele 4 baze adverse.",
            "<b>Familia <code>4P</code> (Echipa A și C vs B și D):</b> Partenerii diagonali câștigă împreună la finalizarea unei ture complete când ocupă simultan ambele baze adverse din colțuri.",
            "<b>Eliminare permanentă în 4P și siguranța bazei partenerului:</b> În <code>4P</code>, dacă un Pod <b>advers</b> își încheie tura completă pe baza ta proprie din colț (sau toate piesele tale Pod sunt capturate și nu mai ai nicio piesă Pod în rezervă), ești <b>eliminat permanent</b> pentru restul partidei: toate piesele tale Pod rămase pe tablă trec în rezerva ta <code>capturedPods</code>, iar <b>toate piesele Prong (normale și SuperProng) de pe ele trec la jucătorul advers care a ocupat baza</b>. Locurile eliminate sunt omise în ordinea turelor, în timp ce partenerul rămas continuă partida. În schimb, <b>intrarea prin pas sau salt pe baza de start liberă a partenerului tău diagonal (sau deja ocupată de propriile tale piese Pod) este legală și fără nicio consecință &mdash; nu îl scoate niciodată pe partener din joc</b> (dacă partenerul tău ocupă temporar baza ta de start în timp ce nu ai nicio piesă Pod pe tablă, tura ta este pur și simplu omisă până când acesta o părăsește).",
            "<b>Eliminare totală / Blocaj:</b> Un jucător fără piese Pod și fără piese Prong în rezervă, ale cărui piese sunt blocate, nu are nicio mutare legală. Dacă jucătorul următor în <code>2P</code> / <code>OFK</code> (sau ambele locuri adverse în <code>4P</code>) nu are mutări legale, jucătorul sau echipa care tocmai a mutat câștigă imediat; în <code>4P</code>, unui jucător blocat al cărui partener are o mutare legală îi este sărită tura. Partidele cu ceas se pot încheia și prin expirarea timpului sau prin cedare.",
            "<b>Remiză prin lipsă de progres (<code>NO_PROGRESS_DRAW</code>):</b> Remiză automată după 30 de mutări consecutive (15 ture complete de fiecare jucător) în <code>2P*</code>/<code>OFK</code> sau 60 de mutări consecutive (15 runde complete) în <code>4P*</code> fără captură, plasare sau eliberare de Pod, inserare de Prong/SuperProng ori eliminare 4P. Se evaluează la finalul turei, după verificările de victorie și blocaj.",
            "Figura 10: Parteneriat 4P (4P): aliații diagonali A(33) și C(77) contra B(37) și D(73), stivă de aceeași culoare [A55ab, A55f], piesa Pod a partenerului C65cf, intrare sigură pe baza partenerului și câmpurile neutre 53/57.",
            "Figura 11: OCTI for Kids (OFK) pe grila 6x7 (34a-36x).",
            "Figura 12: Poziție de victorie 2P_3B cu ocuparea celor 3 baze adverse (A37c, A57a, A77g).",
            "Capitolul 5 &mdash; Tabelul celor 13 variante canonice",
            "Toate platformele folosesc următoarele <b>13 coduri canonice de variantă</b>:",
            "Cod", "Grilă", "Jucători", "Pod (Tablă+Rez.)", "Prong (N/SP)", "Modificatori și obiectiv", "Clasat",
            "Da", "Amical",
            "Reguli standard; ocupă toate cele 3 baze adverse",
            "SuperProng <code>!</code> + deplasări bonus <code>;</code> în linie dreaptă (&le;2); 3 baze",
            "Tablă toroidală fără margini; 3 baze",
            "SuperProng (&le;2 deplasări bonus) + Fără margini; 3 baze",
            "Blitz cu moarte subită; ocupă oricare bază adversă",
            "SuperProng (&le;2 deplasări bonus) + moarte subită la 1 bază",
            "Fără margini + moarte subită la 1 bază",
            "SuperProng (&le;2 deplasări bonus) + Fără margini + 1 bază",
            "Baze neutre <code>53/57</code>, eliminare permanentă; 2 baze adverse",
            "4P pe echipe + deplasări bonus SuperProng (&le;4)",
            "4P pe echipe + tablă fără margini",
            "4P pe echipe + SuperProng (&le;4 deplasări bonus) + Fără margini",
            "OCTI for Kids: 4 baze/parte, 0 rezerve, fără stive/compuse; 1 bază",
            "Capitolul 6 &mdash; Notația oficială a mutărilor și OCTI-FEN",
            "Gramatica notației mutărilor",
            "Sintaxă", "Acțiune", "Exemplu",
            "Plasează un Pod din rezervă pe un câmp OCTI",
            "Eliberează un Pod propriu capturat pe un câmp OCTI ocupat (doar <code>2P_3B*</code> și <code>4P*</code>)",
            "Inserează un Prong normal în orificiul <code>d &isin; {a..h}</code>",
            "Inserează un SuperProng auriu în orificiul <code>d</code>",
            "Pas de 1 câmp fără captură",
            "Salt simplu sau lanț de salturi (<code>x</code> = captură opțională a întregii stive)",
            "Mutare compusă care desparte piese Pod pornite de pe același câmp",
            "1&ndash;4 deplasări bonus SuperProng în linie dreaptă (câte 1 per SP pe tablă; eliminare imediată la <code>x</code>) + tura obișnuită",
            "Formatul șirului de poziție OCTI-FEN",
            "Pozițiile de pe tablă se scriu ca 11 sau 13 elemente separate prin două puncte (plus <code>noProgressPlies</code> opțional):",
            "<code>mode</code>: <code>2</code>, <code>4</code> sau <code>OFK</code> &bull; <code>bases</code>: <code>3</code> sau <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> sau <code>1</code>.",
            "<code>status</code>: <code>N</code> (neîncepută), <code>P</code> (în desfășurare), <code>O</code> (încheiată), <code>R</code> (cedată).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> sau <code>-</code> &bull; <code>elimMask</code>: mască pe biți pentru eliminările 4P (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (de ex., <code>4,0,25,0</code>).",
            "<code>squares</code>: piese Pod separate prin virgulă <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (de ex., <code>A44ab!,B55e!</code>), sau <code>-</code> dacă tabla este goală.",
            "<code>noProgressPlies</code> (al 12-lea/al 14-lea element, opțional): mutări consecutive fără progres (<code>0..1000</code>, implicit <code>0</code>; <code>toPositionString()</code> emite șirul canonic de 11/13 elemente).",
            "Capitolul 7 &mdash; Sistemul Elo pe două categorii și regulile Arenei",
            "Ratingurile oficiale încep de la <b>1200 Elo</b> pe trei variante 1v1: <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b> și <b><code>OFK</code></b>. Partidele 4P, SuperProng, Fără margini, combinate și offline sunt strict amicale.",
            "<b>Categoria 1 &mdash; Clasamente exclusiv umane:</b><br>Cuprinde exclusiv partidele online clasate Om contra Om (<code>K = 32</code>), 100% separate de partidele cu roboți.",
            "<b>Categoria 2 &mdash; Arenă deschisă pentru roboți și oameni:</b><br>Cuprinde partidele online clasate cu cel puțin un robot AI înregistrat, cu trei măsuri anti-abuz:<ul><li><b>Om contra Robot:</b> partide clasate nelimitate la <code>K = 32</code>.</li><li><b>Factori K între roboți:</b> <code>K = 32</code> între proprietari diferiți; <code>K = 16</code> pentru același proprietar.</li><li><b>Limită pe pereche și cedare timpurie:</b> maximum <b>4 partide clasate în 24 de ore</b> per pereche de roboți (ulterior devin amicale) și 0 variație Elo la cedare înainte de <b>8 mutări</b>.</li></ul>"
        },
        /* 5. Russian (ru) */
        {
            "ru",
            {
                { "ch1", "Глава 1", "Геометрия доски, Pod, Prong и начальная расстановка", "Координаты 9×9, начальный резерв и 8 направленных гнёзд a–h" },
                { "ch2", "Глава 2", "Все действия хода", "Вставка Prong, шаг на 1 клетку, цепочки прыжков, резерв и освобождение" },
                { "ch3", "Глава 3", "Стопки, составные ходы и Приложение A", "Составные ходы через запятую, петли A.1, Без границ и SuperProng" },
                { "ch4", "Глава 4", "Победа и выбывание в 4P", "Занятие 3 баз или 1 базы, выбывание в 4P, запирание и ничья без прогресса за 30/60 ходов" },
                { "ch5", "Глава 5", "Таблица 13 канонических вариантов", "Размеры доски, запас Pod/Prong, модификаторы и рейтинговый статус" },
                { "ch6", "Глава 6", "Нотация ходов и грамматика OCTI-FEN", "Официальная алгебраическая нотация и формат OCTI-FEN из 11/13 элементов" },
                { "ch7", "Глава 7", "Два рейтинговых зачёта Elo и правила Арены", "Зачёт 1 — только люди; Зачёт 2 — Открытая арена для роботов и людей" }
            },
            "Официальные иллюстрированные правила OCTI",
            "Полные правила всех вариантов, тактические механики, особые случаи Приложения A, спецификация OCTI-FEN и регламент двух рейтинговых зачётов Elo.",
            "Главы:",
            "Глава 1 &mdash; Геометрия доски, Pod, Prong и начальная расстановка",
            "OCTI &mdash; детерминированная абстрактная стратегическая игра с полной информацией, основанная на построении направлений движения. Каждый Pod выходит на доску пустым, без единого направления, и обретает подвижность по мере вставки Prong, по одному за ход.",
            "Pod (восьмиугольная основная фишка с 8 гнёздами по периметру)",
            "Prong (направляющий штифт, вставляемый в гнездо Pod для открытия направления хода)",
            "SuperProng (особый золотой штифт, дающий один предварительный бонусный ход по прямой перед основным действием)",
            "<b>Стандартная сетка 9&times;9:</b> Вертикали <code>x = 1..9</code> идут с запада на восток; горизонтали <code>y = 1..9</code> &mdash; с юга на север. Каждая клетка записывается двумя цифрами <code>xy</code> (например, <code>53</code> &mdash; 5-я вертикаль, 3-я горизонталь).",
            "<b>Начальные ресурсы для 2 игроков (<code>2P_3B</code> и <code>2P_1B</code>):</b> Игрок A начинает с 3 Pod на домашних базах <code>33</code>, <code>53</code>, <code>73</code>; игрок B &mdash; с 3 Pod на <code>37</code>, <code>57</code>, <code>77</code>. У каждой стороны также есть 4 резервных Pod (всего 7 Pod у игрока) и 25 Prong (или 24 обычных Prong + 1 золотой SuperProng в <code>_SP</code>).",
            "<b>8 гнёзд Prong по компасу (<code>a</code>&ndash;<code>h</code>):</b> По часовой стрелке от севера: <code>a</code>=С, <code>b</code>=СВ, <code>c</code>=В, <code>d</code>=ЮВ, <code>e</code>=Ю, <code>f</code>=ЮЗ, <code>g</code>=З, <code>h</code>=СЗ.",
            "Рисунок 1: Стандартная расстановка 9×9 для 2 игроков (2P_3B), домашние базы 33/53/73 против 37/57/77.",
            "Рисунок 2: В 2P_1B (блиц до 1 базы) расстановка та же, но занятие любой одной базы соперника приносит победу.",
            "Рисунок 3: 8 гнёзд a–h вокруг Pod A55acg и вставка Prong из резерва (55acg+b).",
            "Глава 2 &mdash; Все действия хода",
            "Игрок A ходит первым, затем ходы строго чередуются (в <code>4P</code> очерёдность идёт по кругу <code>A &rarr; B &rarr; C &rarr; D</code>, пропуская выбывшие места). Пропуск хода запрещён; в свой ход вы обязаны выполнить ровно одно основное действие:",
            "<b>Вставить Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Возьмите 1 Prong из резерва и вставьте в пустое гнездо своего Pod на доске (например, <code>55acg+b</code> или <code>33+b!</code>). После установки Prong остаётся на этом Pod навсегда &mdash; переставлять установленный Prong в другое гнездо запрещено во всех вариантах.",
            "<b>Шаг на 1 клетку (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Переместите Pod на 1 клетку по направлению установленного на нём Prong на свободную клетку или в стопку из ваших собственных Pod (например, <code>44ab-45</code>). Шаг на 1 клетку <b>никогда не совершает захват</b> и не позволяет зайти на клетку, занятую Pod другого игрока (включая вашего диагонального партнёра в <code>4P</code>).",
            "<b>Прыжок на 2 клетки и цепочка прыжков (<code>xy&lt;cfg&gt;-x1y1[x]-...</code>):</b> Прыгните на 2 клетки по направлению установленного Prong через соседнюю занятую клетку (свою или соперника) на свободную клетку сразу за ней. Тем же Pod можно продолжить цепочку прыжков по любому из его установленных Prong (меняя направление между прыжками) и остановиться после любого допустимого прыжка.",
            "<b>Выставить резервный Pod (<code>xy</code>) или освободить пленный Pod (<code>xy*</code>):</b> Выставьте 1 резервный Pod на свою домашнюю базу, когда она пуста или занята только вашими собственными Pod (либо на базу соперника, которую вы уже занимаете), или &mdash; в партиях на 3 базы (<code>2P_3B*</code>) и 4 игроков (<code>4P*</code>) &mdash; освободите 1 свой пленный Pod (<code>xy*</code>) на занятую вами клетку OCTI при выполнении условий (освобождение не применяется в блице до 1 базы и в <code>OFK</code>).",
            "<b>Необязательный захват всей стопки и трофейные Prong:</b> При прыжке через занятую клетку захват необязателен: добавьте <code>x</code> (например, <code>44ab-66x</code>), чтобы захватить все Pod в перепрыгнутой стопке целиком (частичный захват запрещён), либо опустите <code>x</code> (<code>44ab-66</code>), чтобы оставить стопку на месте. В обычных одиночных и составных ходах захваченные стопки снимаются в конце хода в резерв <code>capturedPods</code> владельца, а все их Prong переходят в свободный резерв захватившего игрока.",
            "Рисунок 4: Шаг на 1 клетку (44ab-45) и прыжок с необязательным захватом всей стопки (44ab-66x).",
            "Рисунок 5: Цепочка прыжков 33abd-55-57x-75x с поворотами и захватами.",
            "Рисунок 7: Выставление резерва на 33 и освобождение (53* или 57*) в 2P_3B* и 4P*.",
            "Глава 3 &mdash; Стопки, составные ходы и Приложение A",
            "<b>Стопки своих Pod и составные ходы несколькими Pod (<code>,</code>):</b> Несколько Pod вашего собственного цвета могут образовывать стопку на одной клетке (запрещено в <code>OFK</code>; в <code>4P</code> объединение в стопку с другим цветом &mdash; включая вашего диагонального партнёра &mdash; строго запрещено). Любая группа ваших собственных Pod, <b>начинающих ход на одной клетке</b>, может двигаться вместе или разделяться по независимым маршрутам шагов и прыжков в один ход через запятую (например, <code>44a-45,44beg-34</code>). Выставление Pod и вставка Prong никогда не объединяются с движением.",
            "<b>Захват всей стопки и снятие в конце хода:</b> Прыжок через занятую клетку с <code>x</code> захватывает каждый Pod в стопке на этой клетке. В обычных одиночных и составных ходах стопки, перепрыгнутые с <code>x</code>, остаются на доске как опоры для прыжков остальных ваших Pod из той же стопки до конца хода и снимаются вместе по завершении хода. Один Pod никогда не может перепрыгнуть одну и ту же клетку дважды за одну цепочку прыжков.",
            "<b>Приложение A.1 &mdash; Уход со стартовой клетки и петли:</b> Прыгающий Pod не может перепрыгивать ни свою стартовую клетку <code>S</code>, ни любой Pod, с которым начинал ход в одной стопке (<code>44bcd-66-86-16-34-54</code> через <code>A44a</code> запрещён). Он может приземлиться обратно на <code>S</code> только если все Pod, начинавшие ход на <code>S</code>, покинут клетку <code>S</code> в этом ходе (<code>44a-45,44beg-66x-64x-44</code> допустим). Замкнутые прыжки без захвата допустимы и считаются ходами простого перемещения (<code>+1</code> к счётчику ничьей без прогресса).",
            "<b>Условия освобождения Pod (<code>2P_3B*</code> и <code>4P*</code>):</b> <code>xy*</code> освобождает 1 ваш собственный пленный Pod и всегда требует, чтобы целевая клетка OCTI уже была занята хотя бы одним вашим собственным Pod (не пустая и не занятая только партнёром). В <code>2P_3B*</code> для освобождения на свою базу дополнительно нужно занимать хотя бы одну базу соперника; для освобождения прямо на уже занятую вами базу соперника достаточно <code>capturedPods &gt; 0</code>; в <code>4P*</code> для освобождения на свою или партнёрскую угловую базу нужно лично занимать нейтральную (<code>53</code>/<code>57</code>) или вражескую клетку OCTI, и это никогда не возвращает выбывшего партнёра.",
            "<b>Тороидальная доска «Без границ» (<code>_ED</code>):</b> Вертикаль 1 смыкается с 9, а горизонталь 1 &mdash; с 9, включая диагонали, как для шагов на 1 клетку (<code>15g-95</code>), так и для прыжков на 2 клетки (<code>97b-29x</code>).",
            "<b>SuperProng и предварительные бонусные ходы (<code>_SP</code>, <code>;</code>):</b> Золотой SuperProng после установки постоянен. Перед основным действием можно выполнить по 1 прямолинейному предварительному бонусному ходу (шаг на 1 клетку или прямолинейная цепочка прыжков без поворотов) за каждый контролируемый вами SuperProng на доске (до 2 в <code>2P_SP</code> и до 4 в <code>4P_SP</code>, например, <code>44ab!-66x;66ab!-67</code>), каждый по отдельному гнезду SuperProng. Стопки, захваченные с <code>x</code> в бонусном ходе, снимаются немедленно; Pod может затем использовать любой из своих Prong в основной фазе хода, а победа проверяется только по завершении полного хода.",
            "Рисунок 6: Разделение стопки 44a-45,44beg-34 и допустимая петля 44a-45,44beg-66x-64x-44 (Приложение A.1).",
            "Рисунок 8: Шаг через край 15g-95 и диагональный прыжок с захватом 97b-29x на доске «Без границ».",
            "Рисунок 9: Бонусный ход SuperProng 44ab!-66x; с немедленным снятием перед основным шагом 66ab!-67.",
            "Глава 4 &mdash; Победа, выбывание в 4P, запирание и ничья без прогресса",
            "<b>Семейство <code>2P_3B</code>:</b> Победа по завершении вашего полного хода, если ваши Pod одновременно занимают все 3 базы соперника.",
            "<b>Семейство <code>2P_1B</code>:</b> Победа по завершении вашего полного хода, когда ваш Pod занимает любую из 3 баз соперника.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Победа по завершении вашего полного хода, когда ваш Pod занимает любую из 4 баз соперника.",
            "<b>Семейство <code>4P</code> (Команда A и C против B и D):</b> Диагональные партнёры побеждают вместе по завершении полного хода, одновременно занимая обе угловые базы соперников.",
            "<b>Окончательное выбывание в 4P и безопасность базы партнёра:</b> В <code>4P</code>, если <b>вражеский</b> Pod заканчивает полный ход на вашей угловой домашней базе (или все ваши Pod захвачены при нулевом остатке резервных Pod), вы <b>окончательно выбываете</b> до конца партии: все оставшиеся на доске ваши Pod уходят в ваш резерв <code>capturedPods</code>, а <b>все их Prong (обычные и SuperProng) переходят тому сопернику, который занял базу</b>. Выбывшие места пропускаются в очерёдности ходов, пока оставшийся партнёр продолжает борьбу. Напротив, <b>заход шагом или прыжком на свободную стартовую базу вашего диагонального партнёра (или уже занятую вашими собственными Pod) полностью допустим и не влечёт никаких последствий &mdash; это никогда не выбивает партнёра из игры</b> (если же ваш партнёр временно занимает вашу стартовую базу, когда у вас на доске нет ни одного Pod, ваш ход просто пропускается, пока он не покинет её).",
            "<b>Полное уничтожение / запирание:</b> Игрок без резервных Pod и Prong, чьи фишки заблокированы, не имеет допустимых ходов. Если у следующего игрока в <code>2P</code> / <code>OFK</code> (или у обоих соперников в <code>4P</code>) нет допустимых ходов, только что сходивший игрок или команда немедленно побеждает; в <code>4P</code> запертый игрок, чей партнёр ещё имеет допустимый ход, просто пропускает ход. Партии с часами также завершаются при просрочке времени или сдаче.",
            "<b>Ничья без прогресса (<code>NO_PROGRESS_DRAW</code>):</b> Автоматическая ничья после 30 ходов подряд (по 15 полных ходов каждого игрока) в <code>2P*</code>/<code>OFK</code> или 60 ходов подряд (15 полных кругов) в <code>4P*</code> без захвата, выставления или освобождения Pod, вставки Prong/SuperProng либо выбывания в 4P. Проверяется в конце хода после проверки победы и запирания.",
            "Рисунок 10: Командный режим 4P (4P): диагональные союзники A(33) и C(77) против B(37) и D(73), стопка одного цвета [A55ab, A55f], Pod партнёра C65cf, безопасный заход на базу партнёра и нейтральные клетки 53/57.",
            "Рисунок 11: OCTI for Kids (OFK) на сетке 6×7 (34a-36x).",
            "Рисунок 12: Победная позиция 2P_3B с занятием всех 3 баз соперника (A37c, A57a, A77g).",
            "Глава 5 &mdash; Справочная таблица 13 канонических вариантов",
            "Все платформы используют следующие <b>13 канонических кодов вариантов</b>:",
            "Код", "Сетка", "Игроки", "Pod (доска+рез.)", "Prong (обыч./SP)", "Модификаторы и цель победы", "Рейтинг",
            "Да", "Без рейтинга",
            "Стандартные правила; занять все 3 базы соперника",
            "SuperProng <code>!</code> + бонусные ходы по прямой <code>;</code> (&le;2); 3 базы",
            "Тороидальная доска «Без границ»; 3 базы",
            "SuperProng (&le;2 бонусных хода) + Без границ; 3 базы",
            "Блиц до первой базы; занять любую базу соперника",
            "SuperProng (&le;2 бонусных хода) + блиц до 1 базы",
            "Без границ + блиц до 1 базы",
            "SuperProng (&le;2 бонусных хода) + Без границ + 1 база",
            "Нейтральные <code>53/57</code>, выбывание по базе; 2 угловые базы",
            "Команды 4P + бонусные ходы SuperProng по прямой (&le;4)",
            "Команды 4P + доска «Без границ»",
            "Команды 4P + SuperProng (&le;4 бонусных хода) + Без границ",
            "OCTI for Kids: 4 базы на сторону, без резерва, без стопок и составных ходов; 1 база",
            "Глава 6 &mdash; Официальная нотация ходов и спецификация OCTI-FEN",
            "Грамматика записи ходов",
            "Синтаксис", "Действие", "Пример",
            "Выставить резервный Pod на базовую клетку OCTI",
            "Освободить свой пленный Pod на занятую клетку OCTI (только <code>2P_3B*</code> и <code>4P*</code>)",
            "Вставить обычный Prong в гнездо <code>d &isin; {a..h}</code>",
            "Вставить золотой SuperProng в гнездо <code>d</code>",
            "Шаг на 1 клетку без захвата",
            "Одиночный прыжок или цепочка прыжков (<code>x</code> = необязательный захват всей стопки)",
            "Составной ход с разделением Pod со стартовой клетки",
            "1&ndash;4 предварительных бонусных хода SuperProng по прямой (немедленное снятие при <code>x</code>) + основной ход",
            "Формат строки позиции OCTI-FEN",
            "Позиции записываются в 11 или 13 элементов через двоеточие (плюс необязательный элемент <code>noProgressPlies</code>):",
            "<code>mode</code>: <code>2</code>, <code>4</code> или <code>OFK</code> &bull; <code>bases</code>: <code>3</code> или <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> или <code>1</code>.",
            "<code>status</code>: <code>N</code> (не начата), <code>P</code> (идёт партия), <code>O</code> (завершена), <code>R</code> (сдача).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> или <code>-</code> &bull; <code>elimMask</code>: битовая маска выбывания в 4P (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (например, <code>4,0,25,0</code>).",
            "<code>squares</code>: список Pod через запятую <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (например, <code>A44ab!,B55e!</code>), или <code>-</code> для пустой доски.",
            "<code>noProgressPlies</code> (необязательный 12-й/14-й элемент): число ходов подряд без прогресса (<code>0..1000</code>, по умолчанию <code>0</code>; <code>toPositionString()</code> выводит 11/13 элементов).",
            "Глава 7 &mdash; Два рейтинговых зачёта Elo и правила Арены",
            "Официальный рейтинг начинается с <b>1200 Elo</b> и ведётся отдельно в трёх вариантах 1 на 1: <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b> и <b><code>OFK</code></b>. Партии 4P, SuperProng, «Без границ», комбинированные и офлайн играются строго без рейтинга.",
            "<b>Зачёт 1 &mdash; рейтинг только среди людей:</b><br>Учитывает исключительно рейтинговые онлайн-партии «Человек против человека» (<code>K = 32</code>), на 100% изолированно от партий с роботами.",
            "<b>Зачёт 2 &mdash; Открытая арена для роботов и людей:</b><br>Учитывает рейтинговые онлайн-партии с участием хотя бы одного зарегистрированного ИИ-робота с тремя мерами защиты:<ul><li><b>Человек против робота:</b> безлимитная рейтинговая игра при <code>K = 32</code>.</li><li><b>Коэффициенты K между роботами:</b> <code>K = 32</code> для разных владельцев; <code>K = 16</code> для одного владельца.</li><li><b>Лимит на пару и ранняя сдача:</b> не более <b>4 рейтинговых партий за 24 часа</b> для одной пары роботов (с 5-й без рейтинга); сдача до <b>8 ходов</b> не меняет рейтинг Elo.</li></ul>"
        },
        /* 6. Spanish (es) */
        {
            "es",
            {
                { "ch1", "Capítulo 1", "Geometría del tablero, Pod, Prong y preparación", "Coordenadas 9x9, reservas iniciales y 8 ranuras direccionales a–h" },
                { "ch2", "Capítulo 2", "Acciones completas del turno", "Inserción de Prong, paso de 1 casilla, cadena multisalto, reserva y liberación" },
                { "ch3", "Capítulo 3", "Apilamiento, jugadas compuestas y Apéndice A", "Jugadas compuestas con coma, bucles A.1, Sin bordes y SuperProng" },
                { "ch4", "Capítulo 4", "Victoria, bloqueo y eliminación 4P", "Ocupación de 3 o 1 base, eliminación 4P, bloqueo y tablas tras 30/60 jugadas" },
                { "ch5", "Capítulo 5", "Matriz de las 13 variantes canónicas", "Dimensiones, dotación de Pod y Prong, modificadores y validez Elo" },
                { "ch6", "Capítulo 6", "Notación de jugadas y gramática OCTI-FEN", "Gramática algebraica oficial y formato OCTI-FEN de 11/13 campos" },
                { "ch7", "Capítulo 7", "Elo de doble grupo y reglas de Arena", "Grupo 1 solo humanos frente al Grupo 2 Arena abierta de robots y humanos" }
            },
            "Reglamento oficial ilustrado de OCTI",
            "Reglas completas multivariante, mecánicas tácticas, casos límite del Apéndice A, gramática oficial de jugadas y OCTI-FEN, y normativa competitiva Elo de doble grupo.",
            "Capítulos:",
            "Capítulo 1 &mdash; Geometría del tablero, Pod, Prong y preparación inicial",
            "OCTI es un juego determinista de estrategia abstracta con información perfecta basado en la construcción de vectores. Cada Pod entra en el tablero vacío (sin direcciones de movimiento) y adquiere movilidad direccional a medida que los jugadores insertan cada <b>Prong</b>, un turno tras otro.",
            "Pod (pieza octogonal principal con 8 ranuras direccionales)",
            "Prong (clavija direccional que se inserta en un Pod para habilitar movimiento)",
            "SuperProng (Prong dorado que otorga un movimiento preludio adicional en línea recta)",
            "<b>Cuadrícula estándar 9&times;9:</b> Las columnas <code>x = 1..9</code> van de oeste a este (de izquierda a derecha); las filas <code>y = 1..9</code> van de sur a norte (de abajo hacia arriba). Cada casilla se escribe con dos dígitos <code>xy</code> (p.&nbsp;ej., <code>53</code> es la columna 5, fila 3).",
            "<b>Recursos iniciales para 2 jugadores (<code>2P_3B</code> y <code>2P_1B</code>):</b> El jugador A comienza con <b>3 Pod</b> en sus bases iniciales OCTI <code>33</code>, <code>53</code> y <code>73</code>; el jugador B comienza con <b>3 Pod</b> en sus bases iniciales OCTI <code>37</code>, <code>57</code> y <code>77</code>. Cada bando dispone además de <b>4 Pod de reserva</b> (7 Pod en total por jugador) y <b>25 Prong</b> (o <b>24 Prong normales + 1 SuperProng dorado</b> en las variantes <code>_SP</code>).",
            "<b>8 ranuras direccionales de Prong (<code>a</code>&ndash;<code>h</code>):</b> Cada Pod octogonal tiene 8 ranuras perimetrales asignadas en sentido horario desde el norte: <code>a</code>=N (<code>0,+1</code>), <code>b</code>=NE (<code>+1,+1</code>), <code>c</code>=E (<code>+1,0</code>), <code>d</code>=SE (<code>+1,-1</code>), <code>e</code>=S (<code>0,-1</code>), <code>f</code>=SO (<code>-1,-1</code>), <code>g</code>=O (<code>-1,0</code>) y <code>h</code>=NO (<code>-1,+1</code>).",
            "Figura 1: Preparación estándar 9x9 para 2 jugadores (2P_3B) con las filas iniciales 33/53/73 frente a 37/57/77.",
            "Figura 2: 2P_1B (Blitz a 1 base) usa la misma preparación, pero ocupar cualquiera de las bases enemigas (55ab-57x) basta para ganar.",
            "Figura 3: Las 8 ranuras direccionales a–h alrededor del Pod A55acg y la inserción de un Prong de la reserva (55acg+b) en una ranura perimetral vacía.",
            "Capítulo 2 &mdash; Acciones completas del turno",
            "El jugador A mueve primero y los turnos se alternan estrictamente (en <code>4P</code>, el orden de turno rota <code>A &rarr; B &rarr; C &rarr; D</code>, omitiendo los asientos eliminados). Pasar está prohibido; en tu turno debes ejecutar <b>exactamente una</b> acción regular:",
            "<b>Insertar un Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Toma 1 Prong de tu reserva disponible e insértalo en una ranura vacía de uno de tus Pod en el tablero (p.&nbsp;ej., <code>55acg+b</code> o <code>33+b!</code> para un SuperProng dorado). Una vez insertados, los Prong son permanentes en ese Pod &mdash; recolocar un Prong instalado en otra ranura nunca está permitido en ninguna variante.",
            "<b>Paso de 1 casilla (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Mueve un Pod 1 casilla siguiendo una de las direcciones de sus Prong instalados hacia una casilla vacía o una pila de tus propios Pod (p.&nbsp;ej., <code>44ab-45</code>). Un paso de 1 casilla <b>nunca captura</b> y no puede entrar en una casilla ocupada por el Pod de otro jugador (incluido tu compañero diagonal en <code>4P</code>).",
            "<b>Salto de 2 casillas y cadena multisalto (<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> Salta 2 casillas en una dirección de Prong instalada por encima de una casilla adyacente ocupada (propia/aliada o enemiga) hasta la casilla vacía situada detrás. Con ese mismo Pod puedes encadenar saltos adicionales usando cualquiera de sus Prong instalados (girando en mitad de la cadena) y detenerte anticipadamente tras cualquier salto legal.",
            "<b>Desplegar Pod de reserva (<code>xy</code>) o liberar Pod capturado (<code>xy*</code>):</b> Coloca 1 Pod de reserva en tu propia base inicial cuando esté vacía u ocupada únicamente por tus propios Pod (o en una base enemiga que ya ocupes), o &mdash; en partidas a 3 bases (<code>2P_3B*</code>) y de 4 jugadores (<code>4P*</code>) &mdash; libera 1 de tus propios Pod capturados (<code>xy*</code>) en una casilla OCTI que ya ocupes cuando cumplas los requisitos (la liberación no se aplica a las variantes de muerte súbita a 1 base ni a <code>OFK</code>).",
            "<b>Captura opcional de pila completa y botín de Prong:</b> Al saltar sobre una casilla ocupada, la captura es <b>opcional</b>: añade <code>x</code> (p.&nbsp;ej., <code>44ab-66x</code>) para capturar <b>todos los Pod de la pila saltada</b> (captura de pila completa &mdash; la captura parcial o selectiva nunca está permitida), u omite <code>x</code> (p.&nbsp;ej., <code>44ab-66</code>) para dejar intacta la pila saltada. En jugadas regulares simples o compuestas, las pilas capturadas se retiran al final del turno y cada Pod pasa a los Pod capturados (<code>capturedPods</code>) de su propietario, mientras que <b>todos los Prong insertados en ellos pasan a la reserva disponible de Prong del jugador que captura</b>. Los Pod propios o aliados pueden saltarse libremente sin capturarlos.",
            "Figura 4: El Pod A44ab avanzando un paso de 1 casilla al norte (44ab-45) frente a saltar 2 casillas al NE sobre B55e con captura opcional de pila completa (44ab-66x).",
            "Figura 5: Cadena multisalto 33abd-55-57x-75x saltando sobre el Pod propio A44h, girando al norte para capturar B56e y luego al SE para capturar B66c.",
            "Figura 7: Despliegue de un Pod disponible de la reserva en la base propia vacía 33 frente a la liberación de un Pod propio capturado (53* o 57*) en 2P_3B* y 4P* tras ocupar una casilla OCTI enemiga (o neutral en 4P).",
            "Capítulo 3 &mdash; Apilamiento, jugadas compuestas y casos límite del Apéndice A",
            "<b>Apilamiento de Pod propios y jugadas compuestas multi-Pod (<code>,</code>):</b> Varios Pod de tu propio color pueden apilarse en la misma casilla (prohibido en <code>OFK</code>; en <code>4P</code>, nunca se permite apilarse con otro color, incluido tu compañero diagonal). Cualquier subconjunto de tus propios Pod que <b>empiecen el turno en la misma casilla</b> puede moverse junto o dividirse por rutas independientes de paso o salto en un mismo turno, unidas con comas (p.&nbsp;ej., <code>44a-45,44beg-34</code>). La colocación de un Pod y la inserción de un Prong nunca se combinan con el movimiento.",
            "<b>Captura de pila completa y retirada al final del turno:</b> Saltar sobre una casilla ocupada con <code>x</code> captura <b>cada Pod de la pila de esa casilla</b>. En jugadas regulares simples y compuestas, las pilas saltadas con <code>x</code> permanecen físicamente en el tablero como puntos de apoyo saltables para los demás compañeros de pila durante ese mismo turno y se retiran juntas al finalizar el turno. Un mismo Pod <b>nunca puede saltar dos veces sobre exactamente la misma casilla</b> en una misma cadena de saltos.",
            "<b>Apéndice A.1 &mdash; Desocupación de la casilla inicial, saltos en bucle y prohibición de saltar compañeros de pila:</b> Durante toda su cadena de saltos, un Pod en salto <b>no puede saltar sobre su casilla inicial <code>S</code> ni sobre ningún Pod con el que haya empezado apilado</b> (por ello, un salto envolvente Sin bordes <code>_ED</code> como <code>44bcd-66-86-16-34-54</code> por encima de <code>A44a</code> estacionario en <code>44</code> es ilegal). Además, un Pod en salto puede <b>volver a aterrizar en <code>S</code></b> al final de un bucle multisalto <b>únicamente cuando todos los Pod que empezaron el turno en <code>S</code> desocupan <code>S</code></b> durante la jugada (p.&nbsp;ej., el despeje compuesto <code>44a-45,44beg-66x-64x-44</code> es legal porque <code>44</code> queda completamente desocupada, mientras que la jugada simple <code>44beg-66x-64x-44</code> dejando a <code>A44a</code> estacionario en <code>44</code> es ilegal). Los saltos múltiples circulares sin captura (<code>S &rarr; ... &rarr; S</code> cuando <code>S</code> queda completamente desocupada) son legales y cuentan como jugadas de solo movimiento (<code>+1</code> hacia el límite de tablas sin progreso).",
            "<b>Alcance y requisitos de la liberación de Pod (<code>2P_3B*</code> y <code>4P*</code>):</b> Liberar un Pod capturado (<code>xy*</code>) se aplica exclusivamente a partidas de <b>3 bases (<code>2P_3B*</code>) y 4 jugadores (<code>4P*</code>)</b> (nunca en muerte súbita a 1 base ni en <code>OFK</code>), siempre libera 1 de <b>tus propios</b> Pod capturados (<code>capturedPods &gt; 0</code>) y exige siempre que la casilla OCTI de destino esté <b>ya ocupada por al menos uno de tus propios Pod</b> (nunca una casilla vacía ni ocupada solo por tu compañero). En <code>2P_3B*</code>, liberar en tu propia base inicial exige además ocupar al menos una base enemiga; liberar directamente en una base enemiga que ya ocupas solo requiere <code>capturedPods &gt; 0</code>. En <code>4P*</code>, liberar en tu base de esquina o en la de tu compañero exige ocupar personalmente una casilla OCTI neutral (<code>53</code>/<code>57</code>) o enemiga (incluso si tu compañero está eliminado, <code>xy*</code> en su base de esquina libera tu propio Pod y nunca revive a un compañero eliminado).",
            "<b>Conexión toroidal Sin bordes (<code>_ED</code>):</b> La columna <code>1</code> conecta con la columna <code>9</code> y la fila <code>1</code> conecta con la fila <code>9</code> (incluidas las diagonales) tanto para pasos de 1 casilla (<code>15g-95</code>) como para saltos de 2 casillas (<code>97b-29x</code> saltando sobre <code>B18e</code>).",
            "<b>SuperProng y preludios adicionales (<code>_SP</code>, <code>;</code>):</b> Un SuperProng dorado (<code>!</code>) es permanente una vez insertado. Antes de tu acción regular del turno, puedes ejecutar <b>1 movimiento preludio adicional por cada SuperProng que controles en el tablero</b> (hasta <b>2</b> en <code>2P_SP</code> y hasta <b>4</b> en <code>4P_SP</code> cuando se capturan SuperProng enemigos), separados por punto y coma (p.&nbsp;ej., <code>44ab!-66x;66ab!-67</code>). Cada preludio mueve 1 Pod estrictamente en <b>línea recta</b> (paso de 1 casilla o cadena de saltos colineales, sin giros) siguiendo una ranura distinta de SuperProng que aún no haya ejecutado un preludio adicional en este turno. Las pilas saltadas con <code>x</code> durante un preludio adicional de SuperProng se <b>retiran de inmediato</b> tras ese preludio antes de que comience el siguiente preludio de SuperProng o la acción regular. Un Pod que se movió en un preludio adicional puede seguir usando <b>cualquiera</b> de sus Prong en la fase regular del turno, y <b>la victoria solo se comprueba al completarse el turno íntegro</b>.",
            "Figura 6: La pila [A44a, A44beg] dividiéndose mediante 44a-45,44beg-34 o desocupando por completo 44 mediante 44a-45,44beg-66x-64x-44 para que A44beg aterrice legalmente en 44 (Apéndice A.1).",
            "Figura 8: Tablero Sin bordes (_ED) con paso envolvente 15g-95 y salto diagonal con captura 97b-29x a través del borde portal.",
            "Figura 9: Preludio adicional rectilíneo con SuperProng dorado 44ab!-66x; (1 movimiento adicional por SuperProng controlado en el tablero) que retira de inmediato a B55e! antes del paso regular 66ab!-67, con comprobación de victoria al finalizar el turno completo.",
            "Capítulo 4 &mdash; Victoria, eliminación, bloqueo y tablas sin progreso",
            "<b>Familia <code>2P_3B</code> (Estándar a 3 bases):</b> Ganas al completarse tu turno íntegro si tus Pod ocupan simultáneamente <b>las 3</b> bases iniciales enemigas (<code>37</code>, <code>57</code>, <code>77</code> para A; <code>33</code>, <code>53</code>, <code>73</code> para B).",
            "<b>Familia <code>2P_1B</code> (Blitz a 1 base):</b> Ganas al completarse tu turno íntegro cuando un Pod tuyo ocupa <b>cualquiera (1)</b> de las 3 bases iniciales enemigas.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Ganas al completarse tu turno íntegro cuando un Pod tuyo ocupa <b>cualquiera (1)</b> de las 4 bases iniciales enemigas (<code>26, 36, 46, 56</code> para A; <code>22, 32, 42, 52</code> para B).",
            "<b>Familia por parejas <code>4P</code> (Equipo A y C vs. Equipo B y D):</b> Los compañeros diagonales ganan juntos al completarse un turno íntegro cuando Pod aliados ocupan simultáneamente <b>ambas bases de esquina enemigas</b> (<code>37</code> y <code>73</code> para el Equipo A y C; <code>33</code> y <code>77</code> para el Equipo B y D).",
            "<b>Eliminación permanente por ocupación de base en 4P y base segura del compañero:</b> En <code>4P</code>, si un Pod <b>enemigo</b> termina su turno íntegro en tu base inicial de esquina (o todos tus Pod son capturados sin que te queden Pod de reserva), quedas <b>eliminado permanentemente</b> por el resto de la partida: todos tus Pod restantes en el tablero pasan a tus Pod capturados (<code>capturedPods</code>), y <b>todos los Prong (normales y SuperProng) de tus Pod retirados se transfieren al jugador enemigo que ocupó tu base</b>. Los asientos eliminados se omiten en la rotación de turnos mientras el compañero superviviente sigue jugando. Por el contrario, <b>entrar mediante un paso o un salto en la base inicial vacía de tu compañero diagonal (o ya ocupada por tus propios Pod) es legal y no tiene ninguna consecuencia &mdash; nunca elimina a tu compañero de la partida</b> (si tu compañero ocupa temporalmente tu base inicial mientras no tienes ningún Pod en el tablero, tu turno simplemente se omite hasta que la desocupe).",
            "<b>Captura total / Bloqueo:</b> Dado que está prohibido recolocar Prong, un jugador sin Pod de reserva y sin Prong de reserva cuyos Pod en el tablero estén completamente encerrados tiene <b>cero jugadas legales</b>. Si el siguiente jugador en <code>2P</code> / <code>OFK</code> (o ambos asientos enemigos en <code>4P</code>) tiene cero jugadas legales, el jugador o equipo que acaba de mover gana de inmediato (en <code>4P</code>, si un jugador queda bloqueado pero su compañero aún dispone de una jugada legal, se omite su turno). Las partidas con reloj también terminan por tiempo agotado o rendición.",
            "<b>Tablas sin progreso en todas las variantes (<code>NO_PROGRESS_DRAW</code>, <code>noProgressPlies</code>):</b> Una partida termina automáticamente en <b>tablas (<code>NO_PROGRESS_DRAW</code>)</b> cuando se completan <b><code>30</code> jugadas consecutivas (<code>15</code> turnos completos por jugador)</b> en cualquier variante <code>2P*</code> u <code>OFK</code> &mdash; o <b><code>60</code> jugadas consecutivas (<code>15</code> rondas completas)</b> en cualquier variante <code>4P*</code> &mdash; sin ningún <b>evento de progreso</b>: captura de Pod (<code>x</code>, incluidos los preludios adicionales de <code>_SP</code>), despliegue de Pod de reserva (<code>xy</code>) o liberación (<code>xy*</code>), inserción de Prong o SuperProng (<code>+d</code>/<code>+d!</code>), o eliminación permanente de un jugador en <code>4P</code>. Se evalúa al completarse el turno <b>después</b> de comprobar la victoria por ocupación de bases, la captura total y el bloqueo sin jugadas legales (por lo que ganar en la jugada <code>30</code>/<code>60</code> sigue siendo victoria).",
            "Figura 10: Parejas de 4 jugadores (4P): aliados diagonales A(33) y C(77) contra B(37) y D(73), pila del mismo color [A55ab, A55f], Pod del compañero C65cf, entrada segura en la base del compañero y casillas OCTI neutrales 53 y 57.",
            "Figura 11: OCTI for Kids (OFK) en la cuadrícula 6x7: 4 bases por bando, 0 Pod de reserva, Prong permanentes, sin apilamiento y victoria a 1 base (34a-36x).",
            "Figura 12: Posición de victoria en 2P_3B (status = O) con el jugador A ocupando simultáneamente las 3 bases enemigas (A37c, A57a, A77g).",
            "Capítulo 5 &mdash; Matriz de referencia de las 13 variantes canónicas",
            "Toda partida en Web, Escritorio, Android, Archivo y SDK de bots utiliza uno de los siguientes <b>13 códigos de variante canónicos</b>:",
            "Código", "Cuadrícula", "Jugadores", "Pod (Inicio+Res)", "Prong (N/SP)", "Modificadores y objetivo", "Clasificatoria",
            "Sí", "Amistosa",
            "Reglas estándar; ocupar las 3 bases enemigas",
            "SuperProng <code>!</code> + preludios rectilíneos <code>;</code> (&le;2); 3 bases",
            "Conexión toroidal Sin bordes; 3 bases",
            "SuperProng (&le;2 preludios) + Sin bordes; 3 bases",
            "Blitz a muerte súbita; ocupar cualquier base enemiga (1)",
            "SuperProng (&le;2 preludios) + muerte súbita a 1 base",
            "Sin bordes + muerte súbita a 1 base",
            "SuperProng (&le;2 preludios) + Sin bordes + muerte súbita a 1 base",
            "Casillas neutrales <code>53/57</code>, eliminación permanente; 2 bases enemigas",
            "Parejas 4P + preludios rectilíneos de SuperProng (&le;4)",
            "Parejas 4P + conexión toroidal Sin bordes",
            "Parejas 4P + SuperProng (&le;4 preludios) + Sin bordes",
            "OCTI for Kids: 4 bases/bando, 0 reservas, sin pilas ni jugadas compuestas; 1 base gana",
            "Capítulo 6 &mdash; Notación oficial de jugadas y especificación OCTI-FEN",
            "Gramática algebraica de notación de jugadas",
            "Sintaxis", "Acción", "Ejemplo",
            "Desplegar un Pod disponible de la reserva en una casilla OCTI",
            "Liberar un Pod propio capturado en una casilla OCTI ocupada (solo <code>2P_3B*</code> y <code>4P*</code>)",
            "Insertar un Prong normal en la ranura <code>d &isin; {a..h}</code>",
            "Insertar un SuperProng dorado en la ranura <code>d</code>",
            "Paso de 1 casilla sin captura",
            "Salto simple o cadena multisalto (<code>x</code> = captura opcional de toda la pila)",
            "Jugada compuesta que divide varios Pod desde la misma casilla de origen",
            "1&ndash;4 preludios rectilíneos de SuperProng (1 por SP en el tablero; retirada inmediata con <code>x</code>) + turno regular",
            "Formato de cadena de posición OCTI-FEN",
            "Los estados del tablero se serializan en cadenas <b>OCTI-FEN</b> separadas por dos puntos (11 campos para <code>2</code> / <code>OFK</code>; 13 campos para <code>4</code>):",
            "<code>mode</code>: <code>2</code>, <code>4</code> u <code>OFK</code> &bull; <code>bases</code>: <code>3</code> o <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> o <code>1</code>.",
            "<code>status</code>: <code>N</code> (no iniciada), <code>P</code> (en curso), <code>O</code> (partida finalizada), <code>R</code> (rendición).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> o <code>-</code> &bull; <code>elimMask</code>: máscara de bits de eliminación 4P (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (p.&nbsp;ej., <code>4,0,25,0</code>).",
            "<code>squares</code>: lista de Pod separados por comas en orden por filas <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (p.&nbsp;ej., <code>A44ab!,B55e!</code>), o <code>-</code> si está vacío.",
            "<code>noProgressPlies</code> (campo final opcional 12.º/14.º): jugadas consecutivas sin progreso (<code>0..1000</code>, por defecto <code>0</code> si se omite; <code>toPositionString()</code> emite la cadena canónica de 11/13 campos).",
            "Capítulo 7 &mdash; Clasificación Elo de doble grupo y reglas competitivas de Arena",
            "Las puntuaciones oficiales comienzan en <b>1200 Elo</b> en tres clasificaciones independientes 1 contra 1: <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b> y <b><code>OFK</code></b>. Todas las partidas de 4 jugadores (<code>4P*</code>), SuperProng (<code>*_SP</code>), Sin bordes (<code>*_ED</code>), combinadas (<code>*_SPED</code>) y sin conexión son estrictamente amistosas (no clasificatorias).",
            "<b>Grupo 1 &mdash; Clasificaciones solo para humanos (Clasificación y directorio de jugadores):</b><br>Registra exclusivamente partidas clasificatorias en línea Humano contra Humano usando el factor Elo estándar <code>K = 32</code>. Las puntuaciones solo para humanos están 100 % aisladas de las partidas con robots.",
            "<b>Grupo 2 &mdash; Arena abierta de robots y humanos (Clasificación y directorio de robots):</b><br>Registra partidas clasificatorias en línea con al menos un robot de IA registrado (Humano contra Robot y Robot contra Robot) en una escala competitiva unificada bajo tres salvaguardas contra manipulación:<ul><li><b>Humano contra Robot:</b> Juego clasificatorio ilimitado con factor estándar <code>K = 32</code> (incluso contra tus propios robots).</li><li><b>Factores K en Robot contra Robot:</b> Los duelos entre distintos propietarios usan el estándar <code>K = 32</code>; las pruebas propias del mismo propietario usan un factor reducido <code>K = 16</code>.</li><li><b>Límite por par de robots y protección ante rendición temprana:</b> Los duelos Robot contra Robot están limitados a <b>4 partidas clasificatorias por ventana móvil de 24 horas</b> entre cada pareja de robots (en cualquier orden) (la 5.ª y siguientes pasan automáticamente a amistosas; sin límite diario frente a robots rivales distintos), y las rendiciones antes de <b>8 jugadas</b> registran un cambio de 0 Elo.</li></ul>"
        },
        /* 7. German (de) */
        {
            "de",
            {
                { "ch1", "Kapitel 1", "Brettgeometrie, Pods, Prongs & Startaufstellung", "9x9-Koordinaten, Startreserven und 8 Kompassbuchsen a–h" },
                { "ch2", "Kapitel 2", "Die Aktionen eines Zuges", "Prong einsetzen, 1-Feld-Schritt, Mehrfachsprünge, Reserve & Befreiung" },
                { "ch3", "Kapitel 3", "Stapeln, Verbundzüge & Anhang A", "Verbundzüge mit Komma, A.1-Rückkehrschleifen, Randlos & SuperProngs" },
                { "ch4", "Kapitel 4", "Siegbedingungen & 4P-Eliminierung", "3-Basen- vs. 1-Basis-Sieg, 4P-Eliminierung & 30/60-Züge-Remis" },
                { "ch5", "Kapitel 5", "Referenzmatrix der 13 Varianten", "Brettgröße, Pod-/Prong-Vorrat, Modifikatoren & Elo-Wertung" },
                { "ch6", "Kapitel 6", "Zugnotation & OCTI-FEN-Grammatik", "Offizielle algebraische Zuggrammatik und 11/13-Elemente-OCTI-FEN-Format" },
                { "ch7", "Kapitel 7", "Doppel-Pool-Elo & Arena-Regeln", "Pool 1 (nur Menschen) vs. Pool 2 (Offene Roboter- & Menschen-Arena)" }
            },
            "Offizielles illustriertes OCTI-Regelbuch",
            "Vollständige Variantenregeln, taktische Mechaniken, Sonderfälle aus Anhang A, offizielle Zug- und OCTI-FEN-Grammatik sowie Wettbewerbsregeln für das Doppel-Pool-Elo-System.",
            "Kapitel:",
            "Kapitel 1 &ndash; Brettgeometrie, Pods, Prongs &amp; Startaufstellung",
            "OCTI ist ein deterministisches abstraktes Strategiespiel der Vektorkonstruktion mit vollständiger Information. Jeder Pod kommt zunächst leer (ohne Bewegungsrichtungen) aufs Brett und gewinnt erst Zug für Zug an Richtungsbeweglichkeit, indem die Spieler <b>Prongs</b> einsetzen.",
            "Pod (achteckiger Hauptspielstein mit 8 Richtungsbuchsen)",
            "Prong (Richtungsstift, der für Bewegungsrichtungen in einen Pod eingesetzt wird)",
            "SuperProng (goldener Bonus-Prong für geradlinige Auftaktzüge)",
            "<b>9&times;9-Standardgitter:</b> Spalten <code>x = 1..9</code> verlaufen von West nach Ost (links nach rechts); Reihen <code>y = 1..9</code> verlaufen von Süd nach Nord (unten nach oben). Jedes Feld wird zweistellig als <code>xy</code> notiert (z.&nbsp;B. bezeichnet <code>53</code> Spalte 5, Reihe 3).",
            "<b>Startvorrat für 2 Spieler (<code>2P_3B</code> &amp; <code>2P_1B</code>):</b> Spieler A beginnt mit <b>3 Pods</b> auf den eigenen OCTI-Heimatbasen <code>33</code>, <code>53</code> und <code>73</code>; Spieler B beginnt mit <b>3 Pods</b> auf den eigenen OCTI-Heimatbasen <code>37</code>, <code>57</code> und <code>77</code>. Zusätzlich hält jeder Spieler <b>4 Reserve-Pods</b> (insgesamt 7 Pods pro Seite) und <b>25 Prongs</b> (bzw. <b>24 normale Prongs + 1 goldenen SuperProng</b> in <code>_SP</code>-Varianten).",
            "<b>8 Kompass-Prong-Buchsen (<code>a</code>&ndash;<code>h</code>):</b> Jeder achteckige Pod besitzt am Rand 8 im Uhrzeigersinn ab Nord angeordnete Buchsen: <code>a</code>=N (<code>0,+1</code>), <code>b</code>=NO (<code>+1,+1</code>), <code>c</code>=O (<code>+1,0</code>), <code>d</code>=SO (<code>+1,-1</code>), <code>e</code>=S (<code>0,-1</code>), <code>f</code>=SW (<code>-1,-1</code>), <code>g</code>=W (<code>-1,0</code>) und <code>h</code>=NW (<code>-1,+1</code>).",
            "Abbildung 1: Standard-9x9-Aufstellung für 2 Spieler (2P_3B) mit den Heimatreihen 33/53/73 gegen 37/57/77.",
            "Abbildung 2: 2P_1B (1-Basis-Blitz) nutzt dieselbe Aufstellung, doch das Besetzen einer beliebigen gegnerischen Basis (55ab-57x) genügt zum Sieg.",
            "Abbildung 3: Die 8 Kompassbuchsen a–h rund um Pod A55acg und das Einsetzen eines Prongs aus dem Vorrat (55acg+b) in eine freie Randbuchse.",
            "Kapitel 2 &ndash; Die Aktionen eines Zuges",
            "Spieler A zieht zuerst, danach wechseln sich die Züge strikt ab (in <code>4P</code> rotiert die Zugfolge <code>A &rarr; B &rarr; C &rarr; D</code>, wobei eliminierte Plätze übersprungen werden). Passen ist verboten; in deinem Zug musst du <b>genau eine</b> reguläre Zugaktion ausführen:",
            "<b>Prong einsetzen (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Nimm 1 Prong aus deinem freien Vorrat und setze ihn in eine leere Buchse eines eigenen Pods auf dem Spielbrett ein (z.&nbsp;B. <code>55acg+b</code> oder <code>33+b!</code> für einen goldenen SuperProng). Einmal eingesetzte Prongs verbleiben dauerhaft in diesem Pod &ndash; das Umstecken eines eingesetzten Prongs in eine andere Buchse ist in keiner Variante erlaubt.",
            "<b>1-Feld-Schritt (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Bewege einen Pod 1 Feld weit entlang einer seiner eingesetzten Prong-Richtungen auf ein freies Feld oder auf einen Stapel deiner eigenen Pods (z.&nbsp;B. <code>44ab-45</code>). Ein 1-Feld-Schritt <b>schlägt niemals</b> und darf kein Feld betreten, das vom Pod eines anderen Spielers besetzt ist (einschließlich deines Diagonalpartners in <code>4P</code>).",
            "<b>2-Felder-Sprung &amp; Mehrfachsprungkette (<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> Springe entlang einer eingesetzten Prong-Richtung 2 Felder weit über ein benachbartes besetztes Feld (eigen/verbündet oder gegnerisch) auf das dahinterliegende leere Feld. Mit demselben Pod darfst du weitere Sprünge entlang beliebiger seiner eingesetzten Prongs verketten (Richtungswechsel mitten in der Kette erlaubt) und nach jedem legalen Teilsprung vorzeitig anhalten.",
            "<b>Reserve-Pod einsetzen (<code>xy</code>) oder gefangenen Pod befreien (<code>xy*</code>):</b> Setze 1 Reserve-Pod auf deine eigene Heimatbasis, wenn sie frei oder ausschließlich von deinen eigenen Pods besetzt ist (oder auf eine bereits von dir besetzte gegnerische Basis), oder befreie &ndash; in 3-Basen- (<code>2P_3B*</code>) und 4-Spieler-Partien (<code>4P*</code>) &ndash; bei erfüllten Voraussetzungen 1 eigenen gefangenen Pod (<code>xy*</code>) auf ein bereits von dir besetztes OCTI-Feld (Befreiung gilt nicht in 1-Basis-Blitzvarianten oder <code>OFK</code>).",
            "<b>Optionales Schlagen ganzer Stapel &amp; Prong-Erbeutung:</b> Beim Überspringen eines besetzten Feldes ist das Schlagen <b>optional</b>: Hänge <code>x</code> an (z.&nbsp;B. <code>44ab-66x</code>), um <b>alle Pods des übersprungenen Stapels</b> zu schlagen (Ganzstapel-Schlagen &ndash; teilweises oder selektives Schlagen einzelner Pods eines Stapels ist niemals erlaubt), oder lasse <code>x</code> weg (z.&nbsp;B. <code>44ab-66</code>), um den übersprungenen Stapel unversehrt stehenzulassen. Bei regulären Einzel- oder Verbundzügen werden geschlagene Stapel am Ende des Zuges in den Gefangenenvorrat (<code>capturedPods</code>) ihres jeweiligen Besitzers überführt, während <b>alle daran befestigten Prongs in den freien Prong-Vorrat des schlagenden Spielers übergehen</b>.",
            "Abbildung 4: Pod A44ab zieht 1 Feld nach Norden (44ab-45) im Vergleich zum 2-Felder-Sprung nach NO über B55e mit optionalem Ganzstapel-Schlagen (44ab-66x).",
            "Abbildung 5: Mehrfachsprungkette 33abd-55-57x-75x: Sprung über den eigenen Pod A44h, Richtungswechsel nach Norden zum Schlagen von B56e und weiter nach SO zum Schlagen von B66c.",
            "Abbildung 7: Einsatz eines freien Reserve-Pods auf die leere Heimatbasis 33 gegenüber der Befreiung eines eigenen gefangenen Pods (53* oder 57*) in 2P_3B* und 4P*, sobald ein gegnerisches (bzw. neutrales 4P-)OCTI-Feld besetzt ist.",
            "Kapitel 3 &ndash; Stapeln, Verbundzüge &amp; Sonderfälle aus Anhang A",
            "<b>Stapeln eigener Pods &amp; Verbundzüge mit mehreren Pods (<code>,</code>):</b> Mehrere Pods deiner eigenen Farbe dürfen auf demselben Feld gestapelt werden (in <code>OFK</code> verboten; in <code>4P</code> ist das Stapeln mit einer anderen Farbe &ndash; einschließlich deines Diagonalpartners &ndash; niemals erlaubt). Beliebig viele deiner eigenen Pods, die <b>den Zug auf demselben Feld beginnen</b>, dürfen in einem Zug gemeinsam ziehen oder sich auf getrennte Schritt-/Sprungpfade aufteilen, verbunden durch Kommas (z.&nbsp;B. <code>44a-45,44beg-34</code>). Das Einsetzen von Pods und Prongs lässt sich niemals mit Bewegung kombinieren.",
            "<b>Ganzstapel-Schlagen &amp; Entfernen am Zugende:</b> Ein Sprung mit <code>x</code> über ein besetztes Feld schlägt <b>jeden Pod im Stapel dieses Feldes</b>. Bei regulären Einzel- und Verbundzügen bleiben mit <code>x</code> übersprungene Stapel während desselben Zuges als überspringbare Stützpunkte für weitere mitziehende Stapel-Pods auf dem Spielbrett und werden erst am Zugende gemeinsam entfernt. Ein einzelner Pod darf innerhalb einer Sprungkette <b>niemals zweimal über exakt dasselbe Feld springen</b>.",
            "<b>Anhang A.1 &ndash; Räumung des Startfelds, Rückkehrschleifen &amp; Sprungverbot über Startstapel-Pods:</b> Während seiner gesamten Sprungkette darf ein springender Pod <b>weder über sein eigenes Startfeld <code>S</code> noch über einen Pod springen, mit dem er zu Zugbeginn gestapelt war</b> (ein randloser <code>_ED</code>-Umlaufsprung wie <code>44bcd-66-86-16-34-54</code> über den auf <code>44</code> verbliebenen Pod <code>A44a</code> ist daher illegal). Außerdem darf ein springender Pod am Ende einer Mehrfachsprung-Schleife <b>nur dann wieder auf <code>S</code> landen, wenn sämtliche zu Zugbeginn auf <code>S</code> gestandenen Pods während des Zuges <code>S</code> räumen</b> (der Verbundzug <code>44a-45,44beg-66x-64x-44</code> ist legal, weil <code>44</code> vollständig geräumt wird; der Einzelzug <code>44beg-66x-64x-44</code>, bei dem <code>A44a</code> auf <code>44</code> stehenbleibt, ist dagegen illegal). Schlagfreie geschlossene Mehrfachsprünge (<code>S &rarr; ... &rarr; S</code> bei vollständiger Räumung von <code>S</code>) sind legal und zählen als reine Bewegungszüge (<code>+1</code> auf den Zähler für Remis ohne Fortschritt).",
            "<b>Geltungsbereich &amp; Voraussetzungen der Pod-Befreiung (<code>2P_3B*</code> &amp; <code>4P*</code>):</b> Das Befreien eines gefangenen Pods (<code>xy*</code>) gilt ausschließlich in <b>3-Basen- (<code>2P_3B*</code>) und 4-Spieler-Partien (<code>4P*</code>)</b> (niemals in 1-Basis-Blitzvarianten oder <code>OFK</code>), befreit stets 1 <b>eigenen</b> gefangenen Pod (<code>capturedPods &gt; 0</code>) und setzt immer voraus, dass das Ziel-OCTI-Feld <b>bereits von mindestens einem eigenen Pod besetzt ist</b> (niemals auf ein leeres oder nur vom Partner besetztes Feld). In <code>2P_3B*</code> erfordert die Befreiung auf eine eigene Heimatbasis zusätzlich, dass du mindestens eine gegnerische Basis besetzt hältst; für die Befreiung direkt auf eine bereits von dir besetzte gegnerische Basis genügt <code>capturedPods &gt; 0</code>. In <code>4P*</code> erfordert die Befreiung auf die eigene Eckbasis oder die Eckbasis des Partners, dass du selbst ein neutrales (<code>53</code>/<code>57</code>) oder gegnerisches OCTI-Feld besetzt hältst (selbst wenn dein Partner eliminiert ist, befreit <code>xy*</code> auf dessen Eckbasis deinen eigenen Pod und holt niemals einen eliminierten Partner zurück ins Spiel).",
            "<b>Randloser Torus-Übergang (<code>_ED</code>):</b> Spalte <code>1</code> ist nahtlos mit Spalte <code>9</code> und Reihe <code>1</code> mit Reihe <code>9</code> verbunden (einschließlich Diagonalen) &ndash; sowohl für 1-Feld-Schritte (<code>15g-95</code>) als auch für 2-Felder-Sprünge (<code>97b-29x</code> mit Sprung über <code>B18e</code>).",
            "<b>SuperProngs &amp; Bonus-Auftaktzüge (<code>_SP</code>, <code>;</code>):</b> Ein goldener SuperProng (<code>!</code>) verbleibt nach dem Einsetzen dauerhaft im Pod. Vor deiner regulären Zugaktion darfst du <b>1 Bonus-Auftaktzug pro von dir auf dem Spielbrett kontrolliertem SuperProng</b> ausführen (bis zu <b>2</b> in <code>2P_SP</code> und bis zu <b>4</b> in <code>4P_SP</code>, wenn gegnerische SuperProngs erbeutet wurden), getrennt durch Semikolons (z.&nbsp;B. <code>44ab!-66x;66ab!-67</code>). Jeder Auftaktzug bewegt 1 Pod streng <b>geradlinig</b> (1-Feld-Schritt oder kollineare Sprungkette ohne Richtungswechsel) entlang einer jeweils unterschiedlichen SuperProng-Buchse, die in diesem Zug noch keinen Bonus-Auftaktzug ausgelöst hat. Mit <code>x</code> übersprungene Stapel während eines SuperProng-Auftaktzugs werden <b>sofort nach diesem Auftaktzug vom Brett entfernt</b>, noch bevor der nächste SuperProng-Auftaktzug oder die reguläre Zugaktion beginnt! Ein Pod, der bereits in einem Bonus-Auftaktzug gezogen hat, darf in der regulären Zugphase weiterhin <b>jeden beliebigen</b> seiner Prongs nutzen; <b>die Siegbedingungen werden erst nach Abschluss des gesamten Zuges geprüft</b>.",
            "Abbildung 6: Stapel [A44a, A44beg] teilt sich per 44a-45,44beg-34 auf oder räumt 44 vollständig per 44a-45,44beg-66x-64x-44, sodass A44beg legal wieder auf dem geräumten Feld 44 landet (Anhang A.1).",
            "Abbildung 8: Randloser Übergangsschritt (_ED) 15g-95 und diagonaler Übergangssprung mit Schlagen 97b-29x über den Portalrand.",
            "Abbildung 9: Geradliniger SuperProng-Bonus-Auftaktzug 44ab!-66x; (1 Bonuszug pro kontrolliertem SuperProng auf dem Brett) entfernt den geschlagenen B55e! sofort vor dem regulären Schritt 66ab!-67; Siegprüfung erst nach Abschluss des gesamten Zuges.",
            "Kapitel 4 &ndash; Sieg, Eliminierung, Blockade &amp; Remis ohne Fortschritt",
            "<b><code>2P_3B</code>-Familie (3-Basen-Standard):</b> Du gewinnst nach Abschluss deines gesamten Zuges, wenn deine Pods gleichzeitig <b>alle 3</b> gegnerischen Heimatbasen besetzen (<code>37</code>, <code>57</code>, <code>77</code> für A; <code>33</code>, <code>53</code>, <code>73</code> für B).",
            "<b><code>2P_1B</code>-Familie (1-Basis-Blitz):</b> Du gewinnst nach Abschluss deines gesamten Zuges, sobald dein Pod <b>eine beliebige</b> der 3 gegnerischen Heimatbasen besetzt.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Du gewinnst nach Abschluss deines vollständigen Zuges, sobald dein Pod <b>eine beliebige</b> der 4 gegnerischen Heimatbasen besetzt (<code>26, 36, 46, 56</code> für A; <code>22, 32, 42, 52</code> für B).",
            "<b><code>4P</code>-Familie im Teammodus (Team A&amp;C gegen Team B&amp;D):</b> Diagonalpartner gewinnen gemeinsam nach Abschluss eines vollen Zuges, wenn verbündete Pods gleichzeitig <b>beide gegnerischen Eckbasen</b> besetzen (<code>37</code> &amp; <code>73</code> für A&amp;C; <code>33</code> &amp; <code>77</code> für B&amp;D).",
            "<b>Dauerhafte 4P-Eliminierung durch Basisbesetzung &amp; sichere Partnerbasis:</b> Beendet in <code>4P</code> ein <b>gegnerischer</b> Pod seinen vollen Zug auf deiner eigenen Eck-Heimatbasis (oder sind alle deine Pods geschlagen und du hast keine Reserve-Pods mehr), bist du für den Rest der Partie <b>dauerhaft eliminiert</b>: Alle deine verbliebenen Pods auf dem Spielbrett wandern in deinen <code>capturedPods</code>-Vorrat, und <b>sämtliche daran befestigten Prongs (normale Prongs und SuperProngs) gehen an den besetzenden Gegner über</b>. Eliminierte Plätze werden in der Zugfolge übersprungen, während der verbliebene Partner weiterspielt. Umgekehrt <b>ist das Ziehen per Schritt oder Sprung auf die freie (oder bereits von deinen eigenen Pods besetzte) Startbasis deines Diagonalpartners legal und hat keinerlei Folgen &ndash; dein Partner scheidet dadurch niemals aus</b> (besetzt dein Partner vorübergehend deine Startbasis, während du keine Pods mehr auf dem Spielbrett hast, wird dein Zug einfach übersprungen, bis er sie räumt).",
            "<b>Komplettschlag / Blockade (keine legalen Züge):</b> Da das Umstecken von Prongs verboten ist, verfügt ein Spieler ohne Reserve-Pods und ohne Reserve-Prongs, dessen Pods auf dem Spielbrett vollständig eingeschlossen sind, über <b>keinen legalen Zug</b>. Hat in <code>2P</code> / <code>OFK</code> der nächste Spieler am Zug (bzw. in <code>4P</code> beide gegnerischen Plätze) keinen legalen Zug, gewinnt sofort der Spieler bzw. das Team, der/das gerade gezogen hat (hat in <code>4P</code> ein blockierter Spieler keinen legalen Zug, sein Partner aber schon, wird der Zug des blockierten Spielers übersprungen). Partien mit Bedenkzeit enden außerdem durch Zeitüberschreitung oder Aufgabe.",
            "<b>Allgemeines Remis ohne Fortschritt (<code>NO_PROGRESS_DRAW</code>, <code>noProgressPlies</code>):</b> Eine Partie endet automatisch <b>remis (<code>NO_PROGRESS_DRAW</code>)</b>, sobald in einer <code>2P*</code>- oder <code>OFK</code>-Variante <b><code>30</code> aufeinanderfolgende Züge (<code>15</code> volle Züge je Spieler)</b> &ndash; bzw. in einer <code>4P*</code>-Variante <b><code>60</code> aufeinanderfolgende Züge (<code>15</code> volle Runden)</b> &ndash; ohne jedes <b>Fortschrittsereignis</b> abgeschlossen werden: Schlagen eines Pods (<code>x</code>, einschließlich <code>_SP</code>-Bonus-Auftaktzügen), Einsetzen eines Reserve-Pods (<code>xy</code>) oder Befreiung (<code>xy*</code>), Einsetzen eines Prongs/SuperProngs (<code>+d</code>/<code>+d!</code>) oder permanente <code>4P</code>-Spielereliminierung. Die Prüfung erfolgt am Zugende <b>nach</b> der Prüfung auf Basisbesetzungssieg, Komplettschlag und Blockade (ein Sieg im Zug <code>30</code> bzw. <code>60</code> zählt also regulär als Sieg).",
            "Abbildung 10: 4-Spieler-Partnerschaft (4P): Diagonalpartner A(33)&C(77) gegen B(37)&D(73), farbgleicher Stapel [A55ab, A55f], Partner-Pod C65cf, sicheres Betreten der Partnerbasis und neutrale Felder 53/57.",
            "Abbildung 11: OCTI for Kids (OFK) auf dem 6x7-Gitter: 4 Basen pro Seite, 0 Reserve-Pods, permanente Prongs, kein Stapeln, 1-Basis-Sieg (34a-36x).",
            "Abbildung 12: 2P_3B-Siegstellung (status = O), in der Spieler A gleichzeitig alle 3 gegnerischen Basen besetzt hält (A37c, A57a, A77g).",
            "Kapitel 5 &ndash; Referenzmatrix der 13 kanonischen Varianten",
            "Jede Partie über Web, Desktop, Android, Archiv und Bot-SDK verwendet einen von <b>13 kanonischen Variantencodes</b>:",
            "Code", "Gitter", "Spieler", "Pods (Start+Res)", "Prongs (N/SP)", "Modifikatoren &amp; Siegziel", "Gewertet",
            "Ja", "Ungewertet",
            "Standardregeln; alle 3 gegnerischen Basen",
            "SuperProng <code>!</code> + <code>;</code> geradlinige Bonus-Auftaktzüge (&le;2); 3 Basen",
            "Randloser Torus-Übergang; 3 Basen",
            "SuperProng (&le;2 Auftaktzüge) + Randlos; 3 Basen",
            "1-Basis-Blitz; eine beliebige gegnerische Basis",
            "SuperProng (&le;2 Auftaktzüge) + 1-Basis-Blitz",
            "Randlos + 1-Basis-Blitz",
            "SuperProng (&le;2 Auftaktzüge) + Randlos + 1 Basis",
            "Neutrale Felder <code>53/57</code>, dauerhafte Eliminierung durch Basisbesetzung; 2 gegnerische Basen",
            "4P-Partnerschaft + geradlinige SuperProng-Bonus-Auftaktzüge (&le;4)",
            "4P-Partnerschaft + randloser Torus-Übergang",
            "4P-Partnerschaft + SuperProng (&le;4 Auftaktzüge) + Randlos",
            "4 Basen/Seite, 0 Reserve, kein Stapeln, keine Verbundzüge; 1 Basis",
            "Kapitel 6 &ndash; Offizielle Zugnotation &amp; OCTI-FEN-Spezifikation",
            "Grammatik der Zugnotation",
            "Syntax", "Aktion", "Beispiel",
            "Freien Reserve-Pod auf ein OCTI-Feld einsetzen",
            "Eigenen gefangenen Pod auf besetztem OCTI-Feld befreien (nur <code>2P_3B*</code> &amp; <code>4P*</code>)",
            "Normalen Prong in Buchse <code>d &isin; {a..h}</code> einsetzen",
            "Goldenen SuperProng in Buchse <code>d</code> einsetzen",
            "Schlagfreier 1-Feld-Schritt",
            "Einzel- oder Mehrfachsprungkette (<code>x</code> = optionales Ganzstapel-Schlagen)",
            "Verbundzug mehrerer Pods vom selben Startfeld",
            "1&ndash;4 geradlinige SuperProng-Bonus-Auftaktzüge (1 pro SP auf dem Brett; sofortiges Entfernen bei <code>x</code>) + regulärer Zug",
            "OCTI-FEN-Stellungsformat",
            "Spielstellungen werden als durch Doppelpunkte getrennte <b>OCTI-FEN</b>-Zeichenketten serialisiert (11 Elemente für <code>2</code> / <code>OFK</code>; 13 Elemente für <code>4</code>):",
            "<code>mode</code>: <code>2</code>, <code>4</code> oder <code>OFK</code> &bull; <code>bases</code>: <code>3</code> oder <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> oder <code>1</code>.",
            "<code>status</code>: <code>N</code> (nicht gestartet), <code>P</code> (läuft), <code>O</code> (beendet), <code>R</code> (aufgegeben).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> oder <code>-</code> &bull; <code>elimMask</code>: 4P-Eliminierungs-Bitmaske (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (z.&nbsp;B. <code>4,0,25,0</code>).",
            "<code>squares</code>: kommagetrennte Pods in zeilenweiser Ordnung <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (z.&nbsp;B. <code>A44ab!,B55e!</code>) oder <code>-</code> wenn leer.",
            "<code>noProgressPlies</code> (optionales 12. bzw. 14. Element am Ende): aufeinanderfolgende Züge ohne Fortschritt (<code>0..1000</code>, Standardwert <code>0</code> bei Weglassen; kanonisches <code>toPositionString()</code> gibt die 11-/13-Elemente-Zeichenkette aus).",
            "Kapitel 7 &ndash; Doppel-Pool-Elo &amp; Wettkampf-Arena-Regeln",
            "Offizielle Wertungen beginnen bei <b>1200 Elo</b> auf drei unabhängigen 1-gegen-1-Variantenranglisten: <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b> und <b><code>OFK</code></b>. Alle 4-Spieler- (<code>4P*</code>), SuperProng- (<code>*_SP</code>), Randlos- (<code>*_ED</code>), kombinierten (<code>*_SPED</code>) sowie Offline-Partien sind ungewertet.",
            "<b>Pool 1 &ndash; Ranglisten nur für Menschen (Rangliste &amp; Spielerverzeichnis):</b><br>Erfasst ausschließlich gewertete Online-Partien Mensch gegen Mensch mit dem Standard-Elo-Faktor <code>K = 32</code>. Die Wertungen in Pool 1 sind vollständig von Roboter-Partien getrennt.",
            "<b>Pool 2 &ndash; Offene Roboter- &amp; Menschen-Arena (Rangliste &amp; Roboterverzeichnis):</b><br>Erfasst gewertete Online-Partien mit Beteiligung mindestens eines registrierten KI-Roboters (Mensch gegen Roboter sowie Roboter gegen Roboter) unter drei Schutzregeln gegen Wertungsmanipulation:<ul><li><b>Mensch gegen Roboter:</b> Gewertete Partien ohne Begrenzung mit Standardfaktor <code>K = 32</code> (auch gegen deine eigenen Roboter).</li><li><b>K-Faktoren für Roboter gegen Roboter:</b> Partien zwischen verschiedenen Besitzern nutzen <code>K = 32</code>; Selbsttests desselben Besitzers nutzen ein reduziertes <code>K = 16</code>.</li><li><b>Paar-Limit &amp; Frühaufgabe-Schutz:</b> Partien Roboter gegen Roboter sind pro Roboter-Paar (unabhängig von der Reihenfolge) auf <b>4 gewertete Partien je gleitendem 24-Stunden-Fenster</b> begrenzt (ab der 5. Partie automatisch ungewertet; kein Tageslimit gegen verschiedene gegnerische Roboter), und eine Aufgabe vor Ablauf von <b>8 Zügen</b> ändert die Elo-Wertung nicht.</li></ul>"
        },
        /* 8. Japanese (ja) */
        {
            "ja",
            {
                { "ch1", "第1章", "盤面構造、Pod、Prong、および初期配置", "9x9座標、初期リザーブ、8方向ソケットa-hの定義" },
                { "ch2", "第2章", "手番のアクション", "Prong挿入、1マスステップ、連続ジャンプ、リザーブ配置と解放" },
                { "ch3", "第3章", "スタック、複合手、および付録A特殊規定", "カンマ複合手、A.1ループ着地、エッジレス循環、SuperProng" },
                { "ch4", "第4章", "勝利条件と4P脱落ルール", "3ベース／1ベース占領勝利、4P永久脱落、30／60手無進展引き分け" },
                { "ch5", "第5章", "全13標準バリアント一覧表", "盤面サイズ、Pod／Prong数、特殊ルール、レート戦対象" },
                { "ch6", "第6章", "公式棋譜記法とOCTI-FEN文法", "代数棋譜文法および11／13トークンOCTI-FEN形式" },
                { "ch7", "第7章", "2プール制Eloレーティングとアリーナ規定", "人間専用プール1とオープンロボット＆人間アリーナプール2" }
            },
            "OCTI 公式図解ルールブック",
            "全バリアント対応の完全ルール、戦術メカニクス、付録Aの境界事例、公式棋譜・OCTI-FEN文法、および2プール制Elo競技規定。",
            "目次：",
            "第1章 &mdash; 盤面構造、Pod、Prong、および初期配置",
            "OCTIは完全情報・確定型のベクトル構築アブストラクト戦略ボードゲームです。すべてのPodは移動方向を一切持たない空の状態で盤上に出現し、プレイヤーが1手番に1本ずつProngを挿入することで方向別の機動力を獲得していきます。",
            "Pod（周囲に8方向のソケットを持つ八角形のメイン駒）",
            "Prong（Podのソケットに挿入して移動方向を付与する方向ピン）",
            "SuperProng（手番前に直線ボーナス移動を行える金色の特別ピン）",
            "<b>9&times;9 標準グリッド：</b>列 <code>x = 1..9</code> は西から東（左から右）、段 <code>y = 1..9</code> は南から北（下から上）へ並びます。すべてのマスは2桁の数字 <code>xy</code> で表します（例：<code>53</code> は5列3段目）。",
            "<b>2人対戦の初期リソース（<code>2P_3B</code> &amp; <code>2P_1B</code>）：</b>プレイヤーAは自陣のOCTIベース <code>33</code>・<code>53</code>・<code>73</code> に<b>Podを3個</b>、プレイヤーBは自陣のOCTIベース <code>37</code>・<code>57</code>・<code>77</code> に<b>Podを3個</b>配置して開始し、各自<b>リザーブPod 4個</b>（各陣営合計7個）と<b>Prong 25本</b>（<code>_SP</code> バリアントでは<b>通常Prong 24本＋金色のSuperProng 1本</b>）を保有します。",
            "<b>8方向のコンパスProngソケット（<code>a</code>&ndash;<code>h</code>）：</b>各八角形Podの外周には北から時計回りに8つのソケットが配置されています：<code>a</code>=北（<code>0,+1</code>）、<code>b</code>=北東（<code>+1,+1</code>）、<code>c</code>=東（<code>+1,0</code>）、<code>d</code>=南東（<code>+1,-1</code>）、<code>e</code>=南（<code>0,-1</code>）、<code>f</code>=南西（<code>-1,-1</code>）、<code>g</code>=西（<code>-1,0</code>）、<code>h</code>=北西（<code>-1,+1</code>）。",
            "図1：標準9x9・2人対戦（2P_3B）の初期配置（自陣ベース段 33/53/73 対 37/57/77）。",
            "図2：2P_1B（1ベース高速戦）は同一の初期配置で、敵ベースいずれか1箇所の占領（55ab-57x）で勝利となります。",
            "図3：Pod A55acg を囲む8方向のソケット a-h と、リザーブのProngを空ソケットへ挿入する手（55acg+b）。",
            "第2章 &mdash; 手番のアクション",
            "プレイヤーAが先手となり、手番は厳密に交互に進みます（<code>4P</code> では脱落した席をスキップしながら <code>A &rarr; B &rarr; C &rarr; D</code> の順に回ります）。パスは禁止されており、自分の手番では以下の通常アクションのうち<b>必ず1つだけ</b>を実行します：",
            "<b>Prongを挿入（<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>）：</b>手持ちのリザーブからProngを1本取り、盤上の自軍Podの空ソケットに挿入します（例：<code>55acg+b</code>、金色のSuperProngは <code>33+b!</code>）。一度挿入されたProngはそのPodに永久固定され、どのバリアントでも装着済みProngを別ソケットへ移し替えることは一切認められません。",
            "<b>1マスステップ（<code>xy&lt;cfg&gt;-x'y'</code>）：</b>装着済みProngのいずれかの方向に沿ってPodを1マス進め、空マスまたは自分のPodのみのスタックに入ります（例：<code>44ab-45</code>）。1マスステップでは<b>捕獲は一切発生せず</b>、他プレイヤーのPodがいるマス（<code>4P</code> における対角パートナーのPodを含む）へ入ることもできません。",
            "<b>2マスジャンプと連続ジャンプ（<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>）：</b>装着済みProngの方向に沿って、隣接する駒のあるマス（味方・敵を問わない）を飛び越え、そのすぐ先の空マス（2マス先）に着地します。同じPodに装着されている任意のProng方向に沿ってさらに連続ジャンプを繋げることができ（途中で方向転換も可能）、合法なジャンプの後ならどこで止めても構いません。",
            "<b>リザーブPodの配置（<code>xy</code>）または捕獲されたPodの解放（<code>xy*</code>）：</b>自分の自陣ベースが空いているか自分のPodのみが置かれているときにリザーブPodを1個その自陣ベース（またはすでに自分が占領している敵ベース）に配置するか、3ベース戦（<code>2P_3B*</code>）および4人戦（<code>4P*</code>）において条件を満たしている場合、すでに自分が占領しているOCTIマスへ捕獲された自分のPodを1個解放（<code>xy*</code>）します（1ベース即決戦系および <code>OFK</code> に解放ルールはありません）。",
            "<b>任意のスタック全体捕獲とProng奪取：</b>駒のあるマスを飛び越えた際、捕獲は<b>任意</b>です。末尾に <code>x</code> を付けると（例：<code>44ab-66x</code>）<b>飛び越えたスタック内の全Podを一括捕獲</b>し（一部だけの部分捕獲は禁止）、<code>x</code> を省略すると（例：<code>44ab-66</code>）スタックはそのまま盤上に残ります。通常の単体手や複合手では、捕獲されたスタックは手番終了時に各Pod所有者の被捕獲枠（<code>capturedPods</code>）へ移され、<b>装着されていたすべてのProngは捕獲したプレイヤーの手持ちProngリザーブに移ります</b>。",
            "図4：Pod A44ab の北への1マスステップ（44ab-45）と、北東の B55e を飛び越えて任意のスタック全体捕獲を行う2マスジャンプ（44ab-66x）。",
            "図5：連続ジャンプ 33abd-55-57x-75x：味方 A44h を飛び越え、北へ転換して B56e を捕獲し、南東へ転換して B66c を捕獲。",
            "図7：空の自陣ベース33へのリザーブPod配置と、2P_3B*・4P*で敵（または4P中立）OCTIマス占領後に捕獲された自軍Podを解放する手（53* または 57*）。",
            "第3章 &mdash; スタック、複合手、および付録Aの境界事例",
            "<b>自分のPodのスタックと複数Podの複合手（<code>,</code>）：</b>自分の色の複数のPodは同じマスにスタックできます（<code>OFK</code> ではスタック禁止。<code>4P</code> でも対角パートナーを含む別の色のPodとスタックすることは一切できません）。<b>その手番の開始時点で同じマスにいた自分のPod</b>は、その一部または全部を、カンマで繋いで1手番のうちに一緒に移動させることも、別々のステップ／ジャンプ経路へ分割して移動させることもできます（例：<code>44a-45,44beg-34</code>）。Podの配置やProngの挿入を移動と組み合わせることは一切できません。",
            "<b>スタック全体捕獲と手番終了時の除去：</b><code>x</code> 付きで駒のあるマスを飛び越えると<b>そのマスのスタック内の全Pod</b>を捕獲します。通常の単体手および複合手では、<code>x</code> で飛び越えられたスタックもその手番中は同じ開始マスから動く残りの複合手Podの踏み台として盤上に残り、手番終了時にまとめて除去されます。1個のPodが1回の連続ジャンプ中に<b>同じマスを2回飛び越えることは絶対にできません</b>。",
            "<b>付録A.1 &mdash; 開始マスの完全退去、ループ着地、および同スタック飛び越し禁止：</b>連続ジャンプの間、ジャンプ中のPodは<b>自身の開始マス <code>S</code> や、手番開始時に同じスタックにいた他のPodを飛び越えることはできません</b>（したがってエッジレス <code>_ED</code> のワープジャンプ <code>44bcd-66-86-16-34-54</code> で <code>44</code> に静止した <code>A44a</code> を飛び越える手は認められません）。さらに、連続ジャンプの終点として<b>開始マス <code>S</code> に戻れるのは、手番開始時に <code>S</code> にいた全Podがその手の間に <code>S</code> から退去した場合に限られます</b>（複合手 <code>44a-45,44beg-66x-64x-44</code> は <code>44</code> が完全に空になるため合法ですが、単体手 <code>44beg-66x-64x-44</code> で <code>A44a</code> を <code>44</code> に残すのは認められません）。捕獲を伴わない循環連続ジャンプ（<code>S</code> が完全に空になる場合の <code>S &rarr; ... &rarr; S</code>）も合法で、移動のみの手（無進展引き分けカウンター <code>+1</code>）に数えられます。",
            "<b>Pod解放の適用範囲と条件（<code>2P_3B*</code> &amp; <code>4P*</code>）：</b>捕獲されたPodの解放（<code>xy*</code>）は<b>3ベース戦（<code>2P_3B*</code>）と4人戦（<code>4P*</code>）</b>専用であり（1ベース即決戦や <code>OFK</code> では不可）、常に<b>自分自身の</b>捕獲されたPodを1個解放し（<code>capturedPods &gt; 0</code>）、対象のOCTIマスに<b>すでに自分のPodが1個以上存在すること</b>が必要です（空マスやパートナーのみのマスは不可）。<code>2P_3B*</code> で自陣ベースに解放するには敵ベースを少なくとも1箇所占領している必要があり、既に占領中の敵ベース上への解放は <code>capturedPods &gt; 0</code> のみで可能です。<code>4P*</code> で自分またはパートナーの角ベースに解放するには、自分自身が中立（<code>53</code>／<code>57</code>）または敵OCTIマスを占領している必要があり、脱落済みパートナーの角ベースで <code>xy*</code> を行っても自分のPodが解放されるだけで脱落したパートナーは復活しません。",
            "<b>エッジレス・トーラス循環（<code>_ED</code>）：</b>1列と9列、1段と9段が（斜め方向も含めて）ループ接続され、1マスステップ（<code>15g-95</code>）も2マスジャンプ（<code>B18e</code> を飛び越える <code>97b-29x</code>）も盤端を跨いで移動できます。",
            "<b>SuperProngと手番前ボーナス移動（<code>_SP</code>、<code>;</code>）：</b>金色のSuperProng（<code>!</code>）は一度挿入すると永久固定です。通常の手番アクションの前に、<b>盤上にある自分のSuperProng 1本につき1回の手番前ボーナス移動</b>（敵のSuperProngを奪取した場合、<code>2P_SP</code> で最大<b>2回</b>、<code>4P_SP</code> で最大<b>4回</b>）をセミコロン区切りで実行できます（例：<code>44ab!-66x;66ab!-67</code>）。各ボーナス移動では、その手番でまだ発動していない別々のSuperProngソケット方向に沿ってPod 1個を<b>一直線のみ</b>（1マスステップまたは同一直線上の連続ジャンプ、方向転換不可）で動かし、<code>x</code> で飛び越えたスタックは次のボーナス移動や通常アクションの前に<b>即座に除去</b>されます。ボーナス移動で動いたPodも通常アクション段階では装着済みの<b>どのProngでも</b>使用でき、<b>勝利判定は手番全体が完了した時点でのみ行われます</b>。",
            "図6：スタック [A44a, A44beg] の分割移動 44a-45,44beg-34 と、44を完全に空にして合法的に44へ戻る 44a-45,44beg-66x-64x-44（付録A.1）。",
            "図8：エッジレス（_ED）の盤端ワープステップ 15g-95 と斜めワープジャンプ捕獲 97b-29x。",
            "図9：金色のSuperProngによる直線の手番前ボーナス移動 44ab!-66x;（盤上のSuperProng 1本につき1回）で捕獲した B55e! を即時除去し、通常ステップ 66ab!-67 へ続く例（勝利判定は手番完了時）。",
            "第4章 &mdash; 勝利条件、脱落、手詰まり、および無進展引き分け",
            "<b><code>2P_3B</code> ファミリー（3ベース標準戦）：</b>手番全体の完了時点で、自軍Podが敵ベース<b>3箇所すべて</b>（Aは <code>37</code>・<code>57</code>・<code>77</code>、Bは <code>33</code>・<code>53</code>・<code>73</code>）を同時に占領していれば勝利となります。",
            "<b><code>2P_1B</code> ファミリー（1ベース高速戦）：</b>手番全体の完了時点で、自軍Podが敵ベース3箇所のうち<b>いずれか1箇所</b>を占領していれば勝利となります。",
            "<b><code>OFK</code>（OCTI for Kids、6&times;7）：</b>自分の手番完了時に、自軍Podが敵ベース4箇所のうち<b>いずれか1箇所</b>（Aは <code>26, 36, 46, 56</code>、Bは <code>22, 32, 42, 52</code>）を占領していれば勝利となります。",
            "<b><code>4P</code> パートナーシップ・ファミリー（チームA&amp;C 対 チームB&amp;D）：</b>手番全体の完了時点で、同盟ペアのPodが<b>敵チームの2つの角ベース両方</b>（A&amp;Cは <code>37</code> と <code>73</code>、B&amp;Dは <code>33</code> と <code>77</code>）を同時に占領していればペアの共同勝利となります。",
            "<b>4人戦のベース占領による永久脱落とパートナーベースの安全ルール：</b><code>4P</code> において、<b>敵</b>Podがいずれかのプレイヤーの角の自陣ベース上で手番全体を終えた場合（またはそのプレイヤーの全Podが捕獲され、残りのリザーブPodも0個になった場合）、そのプレイヤーは対局終了まで<b>永久脱落</b>となります。盤上に残っていたそのプレイヤーの全Podは本人の被捕獲枠（<code>capturedPods</code>）へ回収され、<b>回収された盤上Podに装着されていたすべてのProng（通常ProngおよびSuperProng）はベースを占領した敵プレイヤーへ移譲</b>されます。脱落した席は手番順からスキップされ、生き残ったパートナーが対局を続行します。一方、<b>対角パートナーの空いている開始ベース（または移動するプレイヤー自身のPodだけがすでに置かれているパートナーの開始ベース）へステップやジャンプで入るのは合法でペナルティは一切なく、それによってパートナーが脱落することは決してありません</b>（盤上にそのプレイヤー自身のPodが0個のときにパートナーがその開始ベースを一時的に占めている場合は、パートナーがそのベースを空けるまでその席の手番がスキップされるだけです）。",
            "<b>全滅／手詰まり（合法手なし）：</b>装着済みProngの移し替えが禁止されているため、リザーブPodが0個かつリザーブProngが0本で、盤上のPodも完全に封鎖されると<b>合法手が0手</b>になります。<code>2P</code>／<code>OFK</code> で次の手番プレイヤー（<code>4P</code> では敵チームの両席）に合法手が1つもない場合、直前に指したプレイヤー／チームが即座に勝利します（<code>4P</code> で片方のプレイヤーが手詰まりでもパートナーに合法手がある場合は、その席の手番のみスキップされます）。持ち時間制の対局では時間切れ負けや投了でも終了します。",
            "<b>共通無進展引き分け（<code>NO_PROGRESS_DRAW</code>、<code>noProgressPlies</code>）：</b><code>2P*</code>／<code>OFK</code> で<b>連続 <code>30</code> 手（各プレイヤー <code>15</code> 手番）</b>、<code>4P*</code> で<b>連続 <code>60</code> 手（<code>15</code> ラウンド）</b>の間、<b>進展イベント</b>（<code>_SP</code> ボーナス移動を含むPod捕獲 <code>x</code>、リザーブPod配置 <code>xy</code> または解放 <code>xy*</code>、Prong／SuperProng挿入 <code>+d</code>／<code>+d!</code>、<code>4P</code> 永久脱落）が一度も発生しない場合、自動的に<b>引き分け（<code>NO_PROGRESS_DRAW</code>）</b>となります。手番完了時にベース占領勝利・全駒捕獲・手詰まり判定の<b>後</b>に評価されるため、<code>30</code>／<code>60</code> 手目での勝利は有効です。",
            "図10：4Pパートナーシップ（4P）：対角同盟 A(33)&C(77) 対 B(37)&D(73)、同色スタック [A55ab, A55f]、パートナーPod C65cf、パートナーベースへの安全な進入、中立 53/57。",
            "図11：6x7盤面のOCTI for Kids（OFK）：各陣営4ベース、リザーブPod 0個、永久固定Prong、スタック禁止、1ベース占領勝利（34a-36x）。",
            "図12：プレイヤーAが敵ベース3箇所すべて（A37c, A57a, A77g）を同時占領した 2P_3B 勝利局面（status = O）。",
            "第5章 &mdash; 全13標準バリアント一覧表",
            "Web・デスクトップ・Android・棋譜アーカイブ・Bot SDKのすべての対局で共通の<b>13種類の標準バリアントコード</b>：",
            "コード", "盤面", "人数", "Pod（初期+リザーブ）", "Prong（通常/SP）", "特殊ルールと勝利条件", "レート戦",
            "対象", "カジュアル",
            "標準ルール；敵ベース3箇所すべてを占領",
            "SuperProng <code>!</code>＋直線ボーナス移動 <code>;</code>（&le;2回）；3ベース",
            "エッジレス・トーラス循環；3ベース",
            "SuperProng（&le;2ボーナス移動）＋エッジレス；3ベース",
            "高速即決戦；敵ベースいずれか1箇所を占領",
            "SuperProng（&le;2ボーナス移動）＋1ベース即決戦",
            "エッジレス＋1ベース即決戦",
            "SuperProng（&le;2ボーナス移動）＋エッジレス＋1ベース",
            "中立 <code>53/57</code>、ベース占領永久脱落；敵ベース2箇所",
            "4Pパートナーシップ＋SuperProng直線ボーナス移動（&le;4回）",
            "4Pパートナーシップ＋エッジレス循環盤面",
            "4Pパートナーシップ＋SuperProng（&le;4ボーナス移動）＋エッジレス",
            "各陣営4ベース、リザーブ0、スタック禁止、複合手禁止；1ベース占領勝利",
            "第6章 &mdash; 公式棋譜記法とOCTI-FEN仕様",
            "棋譜記法の文法",
            "構文", "アクション内容", "記述例",
            "手持ちのリザーブPodをOCTIマスに配置",
            "占領中のOCTIマスに捕獲された自軍Podを解放（<code>2P_3B*</code> &amp; <code>4P*</code> のみ）",
            "ソケット <code>d &isin; {a..h}</code> に通常Prongを挿入",
            "ソケット <code>d</code> に金色のSuperProngを挿入",
            "捕獲なしの1マスステップ",
            "単発または連続ジャンプ（<code>x</code>＝任意のスタック全体捕獲）",
            "同じ開始マスの複数Podを分割移動する複合手",
            "1&ndash;4回の直線SuperProng手番前ボーナス移動（盤上のSP 1本につき1回；<code>x</code> は即時除去）＋通常手番",
            "OCTI-FEN 局面文字列フォーマット",
            "盤面状態はコロン区切りの<b>OCTI-FEN</b>文字列にシリアライズされます（<code>2</code>／<code>OFK</code> は11トークン、<code>4</code> は13トークン）：",
            "<code>mode</code>：<code>2</code>、<code>4</code>、または <code>OFK</code> &bull; <code>bases</code>：<code>3</code> または <code>1</code> &bull; <code>super</code> / <code>edgeless</code>：<code>0</code> または <code>1</code>。",
            "<code>status</code>：<code>N</code>（開始前）、<code>P</code>（対局中）、<code>O</code>（終局）、<code>R</code>（投了）。",
            "<code>turn</code> / <code>winner</code>：<code>A</code>、<code>B</code>、<code>C</code>、<code>D</code>、または <code>-</code> &bull; <code>elimMask</code>：4P脱落ビットマスク（<code>A=1, B=2, C=4, D=8</code>）。",
            "<code>resP</code>：<code>freePods,capturedPods,freeProngs,freeSuperProngs</code>（例：<code>4,0,25,0</code>）。",
            "<code>squares</code>：段優先順のカンマ区切り盤上Podリスト <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code>（例：<code>A44ab!,B55e!</code>）、空盤は <code>-</code>。",
            "<code>noProgressPlies</code>（任意の末尾12番目／14番目トークン）：連続無進展手数（<code>0..1000</code>、省略時はデフォルト <code>0</code>；正規の <code>toPositionString()</code> は11／13トークン文字列を出力）。",
            "第7章 &mdash; 2プール制Eloレーティングと競技アリーナ規定",
            "公式レーティングは初期値<b>1200 Elo</b>から始まり、独立した3つの1対1バリアントラダー（<b><code>2P_3B</code></b>、<b><code>2P_1B</code></b>、<b><code>OFK</code></b>）で集計されます。すべての4人戦（<code>4P*</code>）、SuperProng（<code>*_SP</code>）、エッジレス（<code>*_ED</code>）、組み合わせバリアント（<code>*_SPED</code>）、およびオフライン対局はカジュアル戦（レート変動なし）です。",
            "<b>プール1 &mdash; 人間専用ラダー（ランキング＆プレイヤー一覧）：</b><br>オンラインの人間対人間レート戦のみを標準Elo係数 <code>K = 32</code> で集計します。人間専用レーティングはロボット戦から完全に隔離されています。",
            "<b>プール2 &mdash; オープンロボット＆人間アリーナ（ランキング＆ロボット名鑑）：</b><br>登録済みAIロボットが少なくとも1体参加するオンラインレート戦（人間対ロボットおよびロボット対ロボット）を集計し、3つの不正防止規定を設けています：<ul><li><b>人間対ロボット戦：</b>自作ロボットとの対戦を含め、標準 <code>K = 32</code> で対局数無制限。</li><li><b>ロボット対ロボットのK係数：</b>異なるオーナー間は標準 <code>K = 32</code>、同一オーナーの自己対戦テストは縮小係数 <code>K = 16</code>。</li><li><b>ロボットペア上限と早期投了ガード：</b>同一ロボットペア（先後問わず）のレート戦は<b>直近24時間あたり最大4局</b>（5局目以降は自動的にカジュアル戦へ移行；異なる相手ロボットとの対戦に日次上限はなし）、さらに<b>8手未満</b>での投了はElo変動 0 として記録されます。</li></ul>"
        },
        /* 9. Turkish (tr) */
        {
            "tr",
            {
                { "ch1", "Bölüm 1", "Tahta Geometrisi, Pod, Prong ve Başlangıç Dizilimi", "9x9 koordinatlar, başlangıç yedekleri ve 8 pusula yuvası a-h" },
                { "ch2", "Bölüm 2", "Tur Eylemleri", "Prong takma, 1 karelik adım, çoklu atlama, yedek yerleştirme ve kurtarma" },
                { "ch3", "Bölüm 3", "Yığınlama, Birleşik Hamleler ve Ek A", "Virgüllü çoklu Pod hamleleri, A.1 döngüleri, Sınırsız kenar geçişi ve SuperProng" },
                { "ch4", "Bölüm 4", "Zafer ve 4P Eleme Kuralları", "3 üs ve 1 üs işgali, 4P eleme ve 30/60 hamlelik ilerlemesizlik beraberliği" },
                { "ch5", "Bölüm 5", "13 Standart Varyant Matrisi", "Tahta boyutları, Pod/Prong bütçeleri, değiştiriciler ve dereceli uygunluğu" },
                { "ch6", "Bölüm 6", "Hamle Notasyonu ve OCTI-FEN Dilbilgisi", "Resmi cebirsel hamle dilbilgisi ve 11/13 alanlı OCTI-FEN biçimi" },
                { "ch7", "Bölüm 7", "Çift Havuzlu Elo ve Arena Kuralları", "Havuz 1 (yalnızca insan) ve Havuz 2 (açık robot ve insan arenası) korumaları" }
            },
            "OCTI Resmi Resimli Kural Kitabı",
            "Tüm varyant kuralları, taktik mekanikler, Ek A uç durumları, resmi hamle/OCTI-FEN dilbilgisi ve Çift Havuzlu Elo rekabet kuralları.",
            "Bölümler:",
            "Bölüm 1 &mdash; Tahta Geometrisi, Pod, Prong ve Başlangıç Dizilimi",
            "OCTI, vektör inşasına dayalı, şans unsuru içermeyen tam bilgili soyut bir strateji oyunudur. Her <b>Pod</b> tahtaya boş olarak (sıfır hareket yönüyle) girer ve oyuncular her tur birer <b>Prong</b> taktıkça yönlü hareket kabiliyeti kazanır.",
            "Pod (çevresinde 8 yön yuvası bulunan sekizgen ana oyun taşı)",
            "Prong (hareket yönü kazandırmak için Pod yuvasına takılan yön çivisi)",
            "SuperProng (tur öncesi düz çizgide bonus öncü hareket sağlayan altın Prong)",
            "<b>9&times;9 Standart Izgara:</b> Sütunlar <code>x = 1..9</code> Batıdan Doğuya (soldan sağa); satırlar <code>y = 1..9</code> Güneyden Kuzeye (aşağıdan yukarıya) uzanır. Her kare iki rakamla <code>xy</code> biçiminde yazılır (örneğin <code>53</code>, 5. sütun ve 3. satırdır).",
            "<b>2 Oyunculu Başlangıç Kaynakları (<code>2P_3B</code> ve <code>2P_1B</code>):</b> A Oyuncusu <code>33</code>, <code>53</code> ve <code>73</code> OCTI ana üslerinde <b>3 Pod</b> ile; B Oyuncusu ise <code>37</code>, <code>57</code> ve <code>77</code> OCTI ana üslerinde <b>3 Pod</b> ile başlar. Ayrıca her iki tarafın da <b>4 yedek Pod</b> taşı (oyuncu başına toplam 7 Pod) ve <b>25 Prong</b> çivisi (<code>_SP</code> varyantlarında <b>24 normal Prong + 1 altın SuperProng</b>) vardır.",
            "<b>8 Pusula Prong Yuvası (<code>a</code>&ndash;<code>h</code>):</b> Her sekizgen Pod üzerinde Kuzeyden saat yönünde sıralanan 8 çevre yuvası bulunur: <code>a</code>=K (<code>0,+1</code>), <code>b</code>=KD (<code>+1,+1</code>), <code>c</code>=D (<code>+1,0</code>), <code>d</code>=GD (<code>+1,-1</code>), <code>e</code>=G (<code>0,-1</code>), <code>f</code>=GB (<code>-1,-1</code>), <code>g</code>=B (<code>-1,0</code>) ve <code>h</code>=KB (<code>-1,+1</code>).",
            "Şekil 1: 33/53/73 ile 37/57/77 ana üs sıralarını gösteren standart 9x9 2 Oyunculu başlangıç dizilimi (2P_3B).",
            "Şekil 2: 2P_1B (1 Üs Yıldırım) aynı dizilimi kullanır ancak herhangi bir düşman üssünü işgal etmek (55ab-57x) kazanmak için yeterlidir.",
            "Şekil 3: A55acg Pod taşı çevresindeki 8 pusula yuvası a-h ve boş bir çevre yuvasına yedekten Prong takılması (55acg+b).",
            "Bölüm 2 &mdash; Tur Eylemleri",
            "İlk hamleyi A Oyuncusu yapar ve turlar kesin sırayla ilerler (<code>4P</code> modunda sıra, elenen koltukların sırası geçilerek <code>A &rarr; B &rarr; C &rarr; D</code> biçiminde döner). Pas geçmek yasaktır; kendi turunuzda <b>tam olarak bir</b> normal tur eylemi gerçekleştirmelisiniz:",
            "<b>Prong Takma (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Kullanılabilir yedeğinizden 1 Prong çivisi alın ve tahtadaki kendi Pod taşlarınızdan birinin boş yuvasına takın (örn. <code>55acg+b</code>; veya altın SuperProng için <code>33+b!</code>). Takılan Prong o Pod üzerinde kalıcıdır &mdash; takılı bir Prong çivisinin başka bir yuvaya taşınmasına hiçbir varyantta izin verilmez.",
            "<b>1 Karelik Adım (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Bir Pod taşını takılı Prong yönlerinden biri boyunca boş bir kareye veya yalnızca kendi Pod taşlarınızdan oluşan bir yığına 1 kare ilerletin (örn. <code>44ab-45</code>). 1 karelik adım <b>asla taş esir almaz</b> ve başka bir oyuncunun Pod taşının bulunduğu kareye (<code>4P</code> modundaki çapraz ortağınız dahil) giremez.",
            "<b>2 Kare Atlama ve Çoklu Atlama Zinciri (<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> Takılı bir Prong yönü boyunca bitişikteki dolu bir karenin (kendi/müttefik veya düşman) üzerinden hemen arkasındaki boş kareye 2 kare atlayın. Aynı Pod taşıyla takılı herhangi bir Prong yönünde ek atlamalar zincirleyebilir (zincir ortasında yön değiştirerek) ve herhangi bir yasal atlamadan sonra erken durabilirsiniz.",
            "<b>Yedek Pod Yerleştirme (<code>xy</code>) veya Esir Pod Kurtarma (<code>xy*</code>):</b> Kendi ana üssünüz boşken veya yalnızca kendi Pod taşlarınızla doluyken oraya (ya da halihazırda işgal ettiğiniz bir düşman üssüne) 1 yedek Pod yerleştirin ya da &mdash; 3 Üslü (<code>2P_3B*</code>) ve 4 Oyunculu (<code>4P*</code>) karşılaşmalarda &mdash; koşullar sağlandığında halihazırda işgal ettiğiniz bir OCTI karesine kendi esir düşen Pod taşlarınızdan 1 tanesini kurtarın (<code>xy*</code>) (kurtarma kuralı 1 Üslü ani ölüm varyantlarında veya <code>OFK</code> modunda geçerli değildir).",
            "<b>İsteğe Bağlı Tam Yığın Esir Alma ve Prong Ele Geçirme:</b> Dolu bir karenin üzerinden atlandığında esir alma <b>isteğe bağlıdır</b>: atlanan yığındaki <b>tüm Pod taşlarını</b> esir almak için <code>x</code> ekleyin (örn. <code>44ab-66x</code>; esir alma her zaman yığının tamamını kapsar; kısmi veya seçmeli esir alma yapılamaz) ya da atlanan yığını olduğu gibi bırakmak için <code>x</code> eklemeyin (örn. <code>44ab-66</code>). Normal tekli veya birleşik hamleler sırasında esir alınan yığınlar tur sonunda her Pod sahibinin <code>capturedPods</code> havuzuna kaldırılırken, <b>üzerlerindeki tüm Prong çivileri esir alan oyuncunun kullanılabilir Prong yedeğine geçer</b>.",
            "Şekil 4: A44ab Pod taşının Kuzeye 1 karelik adımı (44ab-45) ile B55e üzerinden KD yönüne 2 kare atlayıp isteğe bağlı tam yığın esir alması (44ab-66x).",
            "Şekil 5: Kendi A44h taşı üzerinden atlayıp Kuzeye dönerek B56e taşını ve ardından GD yönüne dönerek B66c taşını esir alan 33abd-55-57x-75x çoklu atlama zinciri.",
            "Şekil 7: Boş 33 ana üssüne yedek Pod yerleştirme ile 2P_3B* ve 4P* modlarında düşman (veya tarafsız 4P) OCTI karesi işgal edildiğinde kendi esir düşen Pod taşını kurtarma (53* veya 57*).",
            "Bölüm 3 &mdash; Yığınlama, Birleşik Hamleler ve Ek A Uç Durumları",
            "<b>Kendi Taşlarını Yığınlama ve Çoklu Pod Birleşik Hamleleri (<code>,</code>):</b> Kendi renginizdeki birden fazla Pod taşı aynı karede yığın oluşturabilir (<code>OFK</code> modunda yasaktır; <code>4P</code> modunda çapraz ortağınız dahil başka bir renkle yığın oluşturmaya asla izin verilmez). <b>Tura aynı karede başlayan</b> kendi Pod taşlarınızdan istedikleriniz, virgülle ayrılarak tek bir turda birlikte hareket edebilir veya ayrı adım/atlama yollarına ayrılabilir (örn. <code>44a-45,44beg-34</code>). Pod yerleştirme ve Prong takma asla hareketle birleştirilemez.",
            "<b>Tam Yığın Esir Alma ve Tur Sonunda Kaldırma:</b> Dolu bir karenin üzerinden <code>x</code> ile atlamak o karedeki <b>yığında bulunan her Pod taşını</b> esir alır. Normal tekli ve birleşik hamlelerde <code>x</code> ile atlanan yığınlar, aynı tur içinde kalan yığın arkadaşları için üzerinden atlanabilir dayanak olarak tur sonuna kadar tahtada kalır ve tur bittiğinde birlikte kaldırılır. Tek bir Pod tek bir atlama zinciri boyunca <b>aynı karenin üzerinden asla iki kez atlayamaz</b>.",
            "<b>Ek A.1 &mdash; Başlangıç Karesinin Boşaltılması, Döngüsel Geri Dönüş ve Yığın Arkadaşları Üzerinden Atlamama:</b> Bir atlama zinciri süresince atlayan Pod <b>kendi başlangıç karesi <code>S</code> üzerinden veya tura birlikte yığın halinde başladığı herhangi bir Pod üzerinden atlayamaz</b> (bu nedenle Sınırsız <code>_ED</code> tahtada <code>44</code> üzerinde sabit kalan <code>A44a</code> üzerinden geçen <code>44bcd-66-86-16-34-54</code> kenar geçişli atlaması geçersizdir). Ayrıca atlayan bir Pod, çoklu atlama döngüsünün sonunda <b>yalnızca tura <code>S</code> üzerinde başlayan tüm Pod taşları hamle sırasında <code>S</code> karesini boşalttığında</b> tekrar <code>S</code> üzerine inebilir (örneğin <code>44</code> karesini tamamen boşaltan birleşik <code>44a-45,44beg-66x-64x-44</code> hamlesi yasaldır; buna karşılık <code>44</code> üzerinde sabit <code>A44a</code> bırakan tekli <code>44beg-66x-64x-44</code> hamlesi geçersizdir). Esir almayan dairesel çoklu atlamalar (<code>S</code> tamamen boşaltıldığında <code>S &rarr; ... &rarr; S</code>) yasaldır ve yalnızca hareket hamlesi (ilerlemesizlik beraberliği eşiğine <code>+1</code>) sayılır.",
            "<b>Pod Kurtarma Kapsamı ve Koşulları (<code>2P_3B*</code> ve <code>4P*</code>):</b> Esir Pod kurtarma (<code>xy*</code>) yalnızca <b>3 Üslü (<code>2P_3B*</code>) ve 4 Oyunculu (<code>4P*</code>)</b> karşılaşmalarda geçerlidir (1 Üslü ani ölüm veya <code>OFK</code> modlarında asla uygulanmaz), her zaman <b>kendi</b> esir düşen Pod taşlarınızdan 1 tanesini kurtarır (<code>capturedPods &gt; 0</code>) ve hedef OCTI karesinin <b>halihazırda en az bir kendi Pod taşınız tarafından işgal edilmiş olmasını</b> gerektirir (boş veya yalnızca ortağa ait kareye yapılamaz). <code>2P_3B*</code> modunda kendi ana üssünüze kurtarma yapmak ek olarak en az bir düşman üssünü işgal etmenizi gerektirir; halihazırda işgal ettiğiniz bir düşman üssüne doğrudan kurtarma için yalnızca <code>capturedPods &gt; 0</code> yeterlidir. <code>4P*</code> modunda kendi veya ortağınızın köşe üssüne kurtarma yapmak bizzat bir tarafsız (<code>53</code>/<code>57</code>) veya düşman OCTI karesini işgal etmenizi gerektirir (ortağınız elenmiş olsa bile onun köşe üssünde <code>xy*</code> kendi Pod taşınızı kurtarır ve elenen ortağı oyuna geri döndürmez).",
            "<b>Sınırsız Toroidal Kenar Geçişi (<code>_ED</code>):</b> Hem 1 karelik adımlarda (<code>15g-95</code>) hem de 2 kare atlamalarda (<code>B18e</code> üzerinden atlayan <code>97b-29x</code>) <code>1.</code> sütun <code>9.</code> sütuna ve <code>1.</code> satır <code>9.</code> satıra (çaprazlar dahil) bağlanır.",
            "<b>SuperProng ve Bonus Öncü Hareketler (<code>_SP</code>, <code>;</code>):</b> Altın SuperProng (<code>!</code>) takıldıktan sonra kalıcıdır. Normal tur eyleminizden önce, <b>tahtada kontrol ettiğiniz her SuperProng başına 1 bonus öncü hareket</b> (düşman SuperProng çivileri ele geçirildiğinde <code>2P_SP</code> içinde en fazla <b>2</b>, <code>4P_SP</code> içinde en fazla <b>4</b>) noktalı virgülle ayırarak yapabilirsiniz (örn. <code>44ab!-66x;66ab!-67</code>). Her öncü hareket, bu turda henüz bonus öncü hareket yapmamış birbirinden farklı bir SuperProng yuvası boyunca 1 Pod taşını kesinlikle <b>düz bir çizgide</b> (1 karelik adım veya yön değiştirmeden doğrusal atlama zinciri) ilerletir. Bir SuperProng bonus öncü hareketi sırasında <code>x</code> ile atlanan yığınlar, sonraki SuperProng öncü hareketi veya normal eylem başlamadan <b>hemen kaldırılır</b>! Bonus öncü harekette oynayan bir Pod, normal tur aşamasında yine <b>herhangi bir</b> Prong çivisini kullanabilir ve <b>zafer koşulları yalnızca tam turun sonunda denetlenir</b>.",
            "Şekil 6: [A44a, A44beg] yığınının 44a-45,44beg-34 ile ayrılması veya 44a-45,44beg-66x-64x-44 ile 44 karesini tamamen boşaltarak A44beg taşının boşalan 44 karesine yasal inişi (Ek A.1).",
            "Şekil 8: Sınırsız (_ED) tahtada kenar geçişli adım 15g-95 ve sınır ötesi çapraz atlamalı esir alma 97b-29x.",
            "Şekil 9: Altın SuperProng düz çizgi bonus öncü hareketi 44ab!-66x; (tahtada kontrol edilen her SuperProng başına 1 bonus hareket) esir alınan B55e! taşını normal adım 66ab!-67 öncesinde hemen kaldırır; zafer tam tur sonunda denetlenir.",
            "Bölüm 4 &mdash; Zafer, Eleme, Abluka ve İlerlemesizlik Beraberliği Koşulları",
            "<b><code>2P_3B</code> Ailesi (3 Üs Standart):</b> Tam turunuzun sonunda Pod taşlarınız rakibin <b>her 3</b> ana üssünü aynı anda işgal ediyorsa kazanırsınız (A için <code>37</code>, <code>57</code>, <code>77</code>; B için <code>33</code>, <code>53</code>, <code>73</code>).",
            "<b><code>2P_1B</code> Ailesi (1 Üs Yıldırım):</b> Tam turunuzun sonunda Pod taşınız rakibin 3 ana üssünden <b>herhangi birini</b> işgal ettiğinde kazanırsınız.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Tam turunuzun sonunda Pod taşınız rakibin 4 ana üssünden <b>herhangi birini</b> işgal ettiğinde kazanırsınız (A için <code>26, 36, 46, 56</code>; B için <code>22, 32, 42, 52</code>).",
            "<b><code>4P</code> Ortaklık Ailesi (A&amp;C Takımı vs. B&amp;D Takımı):</b> Çapraz ortaklar, tam bir turun sonunda müttefik Pod taşları <b>her iki düşman köşe üssünü</b> aynı anda işgal ettiğinde birlikte kazanır (A&amp;C Takımı için <code>37</code> ve <code>73</code>; B&amp;D Takımı için <code>33</code> ve <code>77</code>).",
            "<b>4 Oyunculu Kalıcı Üs Elemesi ve Güvenli Ortak Üssü:</b> <code>4P</code> modunda bir <b>düşman</b> Pod taşı tam turunu sizin köşe ana üssünüzde bitirirse (veya tüm Pod taşlarınız esir alınır ve hiç yedek Pod taşınız kalmazsa) oyunun geri kalanı için <b>kalıcı olarak elenirsiniz</b>: tahtada kalan tüm Pod taşlarınız kendi <code>capturedPods</code> havuzunuza alınır ve <b>bu tahta Pod taşlarınızdaki tüm Prong çivileri (normal ve SuperProng) üssünüzü işgal eden düşman oyuncuya geçer</b>. Hayatta kalan ortak oynamaya devam ederken elenen koltukların sırası tur döngüsünde geçilir. Buna karşılık, <b>çapraz ortağınızın boş (veya yalnızca kendi Pod taşlarınızın bulunduğu) başlangıç üssüne adım atarak ya da atlayarak girmek yasaldır ve hiçbir olumsuz sonucu yoktur &mdash; ortağınızı asla oyundan elemez</b> (tahtada hiç Pod taşınız yokken ortağınız başlangıç üssünüzde geçici olarak duruyorsa, üssü boşaltana kadar sıranız yalnızca geçilir).",
            "<b>Tam İmha / Abluka:</b> Prong yerini değiştirmek yasak olduğundan, yedek Pod taşı ve yedek Prong çivisi kalmayan ve tahtadaki Pod taşları tamamen sıkışan bir oyuncunun <b>hiç yasal hamlesi yoktur</b>. <code>2P</code> / <code>OFK</code> modunda sıradaki oyuncunun (veya <code>4P</code> modunda her iki rakip koltuğun) hiç yasal hamlesi yoksa az önce hamle yapan oyuncu/takım anında kazanır (<code>4P</code> modunda ortağının hâlâ yasal hamlesi bulunan ablukadaki oyuncunun sırası geçilir). Süreli karşılaşmalar süre bitimi veya terk etme ile de sona erer.",
            "<b>Evrensel İlerlemesizlik Beraberliği (<code>NO_PROGRESS_DRAW</code>, <code>noProgressPlies</code>):</b> Herhangi bir <code>2P*</code> veya <code>OFK</code> varyantında <b>art arda <code>30</code> hamle (oyuncu başına <code>15</code> tam tur)</b> &mdash; veya herhangi bir <code>4P*</code> varyantında <b>art arda <code>60</code> hamle (<code>15</code> tam döngü)</b> &mdash; hiçbir <b>ilerleme olayı</b> olmadan tamamlanırsa karşılaşma otomatik olarak <b>berabere (<code>NO_PROGRESS_DRAW</code>)</b> biter: Pod esir alma (<code>x</code>, <code>_SP</code> bonus öncü hareketleri dahil), yedek Pod yerleştirme (<code>xy</code>) veya kurtarma (<code>xy*</code>), Prong/SuperProng takma (<code>+d</code>/<code>+d!</code>) ya da kalıcı <code>4P</code> oyuncu elemesi. Tur sonunda üs işgali zaferi, tam esir alma imhası ve yasal hamle kalmaması (abluka) kontrollerinden <b>sonra</b> değerlendirilir (dolayısıyla <code>30.</code>/<code>60.</code> hamlede kazanmak yine zafer sayılır).",
            "Şekil 10: 4 Oyunculu Ortaklık (4P): çapraz müttefikler A(33)&C(77) ile B(37)&D(73), aynı renk yığın [A55ab, A55f], ortağın Pod taşı C65cf, ortak üssüne güvenli giriş ve tarafsız 53/57.",
            "Şekil 11: 6x7 ızgarada OCTI for Kids (OFK): taraf başına 4 üs, 0 yedek Pod, kalıcı Prong, yığınlama yok, 1 üs zaferi (34a-36x).",
            "Şekil 12: A Oyuncusunun her 3 düşman üssünü (A37c, A57a, A77g) aynı anda işgal ettiği 2P_3B zafer konumu (status = O).",
            "Bölüm 5 &mdash; 13 Standart Varyant Başvuru Matrisi",
            "Web, Masaüstü, Android, Arşiv ve Bot SDK genelindeki her karşılaşma aşağıdaki <b>13 standart varyant kodundan</b> birini kullanır:",
            "Kod", "Izgara", "Oyuncular", "Pod (Başl.+Yedek)", "Prong (N/SP)", "Değiştiriciler ve Zafer Hedefi", "Dereceli",
            "Evet", "Dostluk",
            "Standart kurallar; her 3 düşman üssünü işgal et",
            "SuperProng <code>!</code> + <code>;</code> düz bonus öncü hareketler (&le;2); 3 üs",
            "Sınırsız toroidal kenar geçişi; 3 üs",
            "SuperProng (&le;2 öncü hareket) + Sınırsız; 3 üs",
            "Ani ölüm yıldırım; herhangi bir düşman üssünü işgal et",
            "SuperProng (&le;2 öncü hareket) + 1 üs ani ölüm",
            "Sınırsız + 1 üs ani ölüm",
            "SuperProng (&le;2 öncü hareket) + Sınırsız + 1 üs",
            "Tarafsız <code>53/57</code>, üs işgalinde kalıcı eleme; 2 düşman üssü",
            "4P Ortaklık + düz SuperProng bonus öncü hareketler (&le;4)",
            "4P Ortaklık + Sınırsız kenar geçişi",
            "4P Ortaklık + SuperProng (&le;4 öncü hareket) + Sınırsız",
            "Taraf başına 4 üs, 0 yedek, yığınlama ve birleşik hamle yok; 1 üs",
            "Bölüm 6 &mdash; Resmi Hamle Notasyonu ve OCTI-FEN Belirtimi",
            "Hamle Notasyonu Dilbilgisi",
            "Sözdizimi", "Eylem", "Örnek",
            "OCTI karesine yedek Pod yerleştir",
            "İşgal edilmiş OCTI karesine kendi esir Pod taşını kurtar (yalnızca <code>2P_3B*</code> ve <code>4P*</code>)",
            "<code>d &isin; {a..h}</code> yuvasına normal Prong tak",
            "<code>d</code> yuvasına altın SuperProng tak",
            "Esir almayan 1 karelik adım",
            "Tekli veya çoklu atlama zinciri (<code>x</code> = isteğe bağlı tam yığın esir alma)",
            "Aynı başlangıç karesindeki Pod taşlarını ayıran birleşik hamle",
            "1&ndash;4 düz SuperProng bonus öncü hareket (tahtadaki her SP başına 1; anında <code>x</code> kaldırma) + normal hamle",
            "OCTI-FEN Konum Dizgesi Biçimi",
            "Tahta durumları iki nokta üst üste ile ayrılmış <b>OCTI-FEN</b> dizgelerine dönüştürülür (<code>2</code> / <code>OFK</code> için 11 alan; <code>4</code> için 13 alan):",
            "<code>mode</code>: <code>2</code>, <code>4</code> veya <code>OFK</code> &bull; <code>bases</code>: <code>3</code> veya <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> veya <code>1</code>.",
            "<code>status</code>: <code>N</code> (başlamadı), <code>P</code> (devam ediyor), <code>O</code> (oyun bitti), <code>R</code> (terk etti).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> veya <code>-</code> &bull; <code>elimMask</code>: 4P eleme bit maskesi (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (örn. <code>4,0,25,0</code>).",
            "<code>squares</code>: satır öncelikli sırada virgülle ayrılmış Pod taşları <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (örn. <code>A44ab!,B55e!</code>) veya boşsa <code>-</code>.",
            "<code>noProgressPlies</code> (isteğe bağlı sondaki 12./14. alan): ilerlemesiz ardışık hamle sayısı (<code>0..1000</code>, belirtilmezse varsayılan <code>0</code>; standart <code>toPositionString()</code> 11/13 alanlı dizgeyi üretir).",
            "Bölüm 7 &mdash; Çift Havuzlu Elo ve Rekabetçi Arena Kuralları",
            "Resmi puanlar üç bağımsız 1v1 varyant klasmanında (<b><code>2P_3B</code></b>, <b><code>2P_1B</code></b> ve <b><code>OFK</code></b>) <b>1200 Elo</b> puanından başlar. Tüm 4 Oyunculu (<code>4P*</code>), SuperProng (<code>*_SP</code>), Sınırsız (<code>*_ED</code>), karma (<code>*_SPED</code>) ve çevrimdışı karşılaşmalar dostluk (derecesiz) niteliğindedir.",
            "<b>Havuz 1 &mdash; Yalnızca İnsan Klasmanları (Sıralama ve Oyuncu Listesi):</b><br>Standart Elo katsayısı <code>K = 32</code> kullanarak yalnızca çevrimiçi dereceli İnsan-İnsan karşılaşmalarını izler. Yalnızca İnsan puanları robot karşılaşmalarından tamamen yalıtılmıştır.",
            "<b>Havuz 2 &mdash; Açık Robot ve İnsan Arenası (Sıralama ve Robotlar Dizini):</b><br>En az bir kayıtlı yapay zekâ robotunun yer aldığı çevrimiçi dereceli karşılaşmaları (İnsan-Robot ve Robot-Robot) üç puan istismarı önlemiyle izler:<ul><li><b>İnsan-Robot Karşılaşmaları:</b> Standart <code>K = 32</code> katsayısıyla sınırsız dereceli oyun (kendi robotlarınıza karşı oynananlar dahil).</li><li><b>Robot-Robot K Katsayıları:</b> Farklı sahipler arasındaki karşılaşmalarda standart <code>K = 32</code>; aynı sahibin kendi robotlarını denediği karşılaşmalarda düşürülmüş <code>K = 16</code> uygulanır.</li><li><b>Robot-Robot Çift Sınırı ve Erken Terk Etme Koruması:</b> Sırası fark etmeksizin herhangi bir robot çifti arasındaki Robot-Robot karşılaşmaları <b>kayan 24 saatlik pencerede en fazla 4 dereceli oyunla</b> sınırlandırılmıştır (5. ve sonraki oyunlar otomatik olarak dostluk karşılaşmasına dönüşür; farklı rakip robotlara karşı günlük sınır yoktur) ve <b>8 hamleden</b> önceki terk etmeler 0 Elo değişimi kaydeder.</li></ul>"
        },
        /* 10. Brazilian Portuguese (pt-BR) */
        {
            "pt-BR",
            {
                { "ch1", "Capítulo 1", "Geometria do tabuleiro, Pod, Prong e preparação", "Coordenadas 9x9, reservas iniciais e 8 encaixes direcionais a–h" },
                { "ch2", "Capítulo 2", "As ações de um turno", "Inserção de Prong, passo de 1 casa, cadeia de saltos múltiplos, reserva e libertação" },
                { "ch3", "Capítulo 3", "Empilhamento, lances compostos e Apêndice A", "Lances multi-Pod com vírgula, laços A.1, Sem bordas e SuperProng" },
                { "ch4", "Capítulo 4", "Vitória e eliminação 4P", "Ocupação de 3 bases ou de 1 base, eliminação 4P e empate sem progresso em 30/60 lances" },
                { "ch5", "Capítulo 5", "Matriz das 13 variantes canônicas", "Dimensões, estoque de Pod e Prong, modificadores e elegibilidade Elo" },
                { "ch6", "Capítulo 6", "Notação de lances e gramática OCTI-FEN", "Gramática algébrica oficial e formato OCTI-FEN de 11/13 campos" },
                { "ch7", "Capítulo 7", "Pontuação Elo em dois grupos e Arena", "Proteções do Grupo 1 (somente humanos) e do Grupo 2 (Arena aberta de robôs e humanos)" }
            },
            "Livro de regras oficial ilustrado do OCTI",
            "Regras completas de todas as variantes, mecânicas táticas, casos especiais do Apêndice A, gramática oficial de lances e OCTI-FEN, e regulamento competitivo do Elo em dois grupos.",
            "Capítulos:",
            "Capítulo 1 &mdash; Geometria do tabuleiro, Pod, Prong e preparação inicial",
            "O OCTI é um jogo determinístico de estratégia abstrata de informação perfeita baseado na construção de vetores. Cada Pod começa em branco ao entrar no tabuleiro (sem direções de movimento) e adquire mobilidade direcional à medida que os jogadores inserem um <b>Prong</b> a cada turno.",
            "Pod (peça octogonal principal com 8 encaixes direcionais)",
            "Prong (pino direcional inserido em um Pod para liberar movimento)",
            "SuperProng (Prong dourado que concede um movimento prelúdio bônus em linha reta)",
            "<b>Grade padrão 9&times;9:</b> As colunas <code>x = 1..9</code> vão de Oeste a Leste (da esquerda para a direita); as linhas <code>y = 1..9</code> vão de Sul a Norte (de baixo para cima). Cada casa é escrita com dois dígitos <code>xy</code> (por exemplo, <code>53</code> é a coluna 5, linha 3).",
            "<b>Recursos iniciais para 2 jogadores (<code>2P_3B</code> e <code>2P_1B</code>):</b> O Jogador A começa com <b>3 Pod</b> nas bases iniciais OCTI <code>33</code>, <code>53</code> e <code>73</code>; o Jogador B começa com <b>3 Pod</b> nas bases iniciais OCTI <code>37</code>, <code>57</code> e <code>77</code>. Cada jogador também dispõe de <b>4 Pod na reserva</b> (7 Pod no total por lado) e <b>25 Prong</b> (ou <b>24 Prong normais + 1 SuperProng dourado</b> nas variantes <code>_SP</code>).",
            "<b>8 encaixes direcionais de Prong (<code>a</code>&ndash;<code>h</code>):</b> Cada Pod octogonal possui 8 encaixes no perímetro dispostos no sentido horário a partir do Norte: <code>a</code>=N (<code>0,+1</code>), <code>b</code>=NE (<code>+1,+1</code>), <code>c</code>=L (<code>+1,0</code>), <code>d</code>=SE (<code>+1,-1</code>), <code>e</code>=S (<code>0,-1</code>), <code>f</code>=SO (<code>-1,-1</code>), <code>g</code>=O (<code>-1,0</code>) e <code>h</code>=NO (<code>-1,+1</code>).",
            "Figura 1: Preparação padrão 9x9 para 2 jogadores (2P_3B) com as fileiras iniciais 33/53/73 contra 37/57/77.",
            "Figura 2: 2P_1B (Blitz de 1 base) usa a mesma preparação, mas infiltrar qualquer uma das bases inimigas (55ab-57x) já basta para vencer.",
            "Figura 3: Os 8 encaixes direcionais a–h ao redor do Pod A55acg e a inserção de um Prong da reserva (55acg+b) em um encaixe vazio do perímetro.",
            "Capítulo 2 &mdash; As ações de um turno",
            "O Jogador A joga primeiro e os turnos se alternam estritamente (em <code>4P</code>, a ordem dos turnos gira <code>A &rarr; B &rarr; C &rarr; D</code>, excluindo os assentos eliminados). Passar a vez é proibido; no seu turno, você deve executar <b>exatamente uma</b> ação regular do turno:",
            "<b>Inserir um Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Pegue 1 Prong da sua reserva disponível e insira-o em um encaixe vazio de um dos seus Pod no tabuleiro (ex.: <code>55acg+b</code>; ou <code>33+b!</code> para um SuperProng dourado). Uma vez inserido, o Prong é permanente naquele Pod &mdash; reposicionar um Prong instalado para outro encaixe nunca é permitido em nenhuma variante.",
            "<b>Passo de 1 casa (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Mova um Pod 1 casa na direção de um dos seus Prong instalados para uma casa vazia ou para uma pilha de seus próprios Pod (ex.: <code>44ab-45</code>). Um passo de 1 casa <b>nunca captura</b> e não pode entrar em uma casa ocupada pelo Pod de outro jogador (incluindo o seu parceiro diagonal no <code>4P</code>).",
            "<b>Salto de 2 casas e cadeia de saltos múltiplos (<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> Salte 2 casas na direção de um Prong instalado por cima de uma casa adjacente ocupada (própria/aliada ou inimiga) até a casa vazia logo atrás dela. Com esse mesmo Pod, você pode encadear saltos adicionais usando qualquer um dos seus Prong instalados (mudando de direção no meio da cadeia) e parar antecipadamente após qualquer salto legal.",
            "<b>Colocar Pod da reserva (<code>xy</code>) ou libertar Pod capturado (<code>xy*</code>):</b> Coloque 1 Pod da reserva na sua própria base inicial quando ela estiver vazia ou ocupada apenas pelos seus próprios Pod (ou em uma base inimiga que você já ocupe), ou &mdash; em partidas de 3 bases (<code>2P_3B*</code>) e de 4 jogadores (<code>4P*</code>) &mdash; liberte 1 dos seus próprios Pod capturados (<code>xy*</code>) em uma casa OCTI que você já ocupe quando cumprir os requisitos (a libertação não se aplica às variantes de morte súbita de 1 base nem ao <code>OFK</code>).",
            "<b>Captura opcional da pilha inteira e tomada de Prong:</b> Ao saltar sobre uma casa ocupada, a captura é <b>opcional</b>: adicione <code>x</code> (ex.: <code>44ab-66x</code>) para capturar <b>todos os Pod da pilha saltada</b> (captura da pilha inteira &mdash; a captura seletiva ou parcial nunca é permitida), ou omita <code>x</code> (ex.: <code>44ab-66</code>) para deixar a pilha saltada intacta. Durante lances regulares simples ou compostos, as pilhas capturadas são removidas ao final do turno e cada Pod passa para os Pod capturados (<code>capturedPods</code>) de seu respectivo dono, enquanto <b>todos os Prong encaixados neles passam para a reserva disponível de Prong do jogador que realizou a captura</b>. Pod próprios ou aliados podem ser saltados livremente sem captura.",
            "Figura 4: O Pod A44ab avançando um passo de 1 casa para o Norte (44ab-45) contra um salto de 2 casas para NE sobre B55e com captura opcional da pilha inteira (44ab-66x).",
            "Figura 5: Cadeia de saltos múltiplos 33abd-55-57x-75x saltando o Pod próprio A44h, mudando para o Norte para capturar B56e e depois para SE para capturar B66c.",
            "Figura 7: Colocação de um Pod da reserva na base inicial vazia 33 versus libertação de um Pod próprio capturado (53* ou 57*) em 2P_3B* e 4P* após ocupar uma casa OCTI inimiga (ou neutra no 4P).",
            "Capítulo 3 &mdash; Empilhamento, lances compostos e casos especiais do Apêndice A",
            "<b>Empilhamento de Pod próprios e lances compostos multi-Pod (<code>,</code>):</b> Vários Pod da sua própria cor podem empilhar-se na mesma casa (proibido no <code>OFK</code>; no <code>4P</code>, empilhar com outra cor &mdash; incluindo o seu parceiro diagonal &mdash; nunca é permitido). Um ou mais dos seus próprios Pod que <b>iniciem o turno na mesma casa</b> podem mover-se juntos ou dividir-se por caminhos independentes de passo ou salto em um único turno, unidos por vírgulas (ex.: <code>44a-45,44beg-34</code>). A entrada de Pod e a inserção de Prong nunca se combinam com movimento.",
            "<b>Captura da pilha inteira e remoção ao final do turno:</b> Saltar sobre uma casa ocupada com <code>x</code> captura <b>cada Pod presente na pilha daquela casa</b>. Nos lances regulares simples e compostos, as pilhas saltadas com <code>x</code> permanecem fisicamente no tabuleiro como pontos de apoio para salto dos demais companheiros de pilha durante o mesmo turno e são removidas juntas ao final do turno. Um mesmo Pod <b>nunca pode saltar duas vezes sobre exatamente a mesma casa</b> durante uma única cadeia de saltos.",
            "<b>Apêndice A.1 &mdash; Desocupação da casa inicial, saltos em laço e proibição de saltar companheiros de pilha:</b> Durante toda a sua cadeia de saltos, um Pod em salto <b>não pode saltar sobre a sua casa inicial <code>S</code> nem sobre nenhum Pod com o qual tenha iniciado o turno empilhado</b> (portanto, um salto toroidal Sem bordas <code>_ED</code> como <code>44bcd-66-86-16-34-54</code> por cima do <code>A44a</code> estacionário em <code>44</code> é ilegal). Além disso, um Pod em salto só pode <b>pousar de volta em <code>S</code></b> ao final de um laço de saltos múltiplos <b>quando todos os Pod que iniciaram o turno em <code>S</code> desocuparem <code>S</code></b> durante o lance (por exemplo, o esvaziamento composto <code>44a-45,44beg-66x-64x-44</code> é legal porque <code>44</code> fica totalmente desocupada, enquanto o lance simples <code>44beg-66x-64x-44</code> deixando <code>A44a</code> estacionário em <code>44</code> é ilegal). Laços circulares de saltos múltiplos sem captura (<code>S &rarr; ... &rarr; S</code> quando <code>S</code> fica totalmente desocupada) continuam legais e contam como lances apenas de movimento (<code>+1</code> rumo ao limite de empate sem progresso).",
            "<b>Escopo e requisitos da libertação de Pod (<code>2P_3B*</code> e <code>4P*</code>):</b> Libertar um Pod capturado (<code>xy*</code>) aplica-se exclusivamente às partidas de <b>3 bases (<code>2P_3B*</code>) e de 4 jogadores (<code>4P*</code>)</b> (nunca em morte súbita de 1 base nem em <code>OFK</code>), sempre liberta 1 dos <b>seus próprios</b> Pod capturados (<code>capturedPods &gt; 0</code>) e exige sempre que a casa OCTI de destino <b>já esteja ocupada por pelo menos um dos seus próprios Pod</b> (nunca uma casa vazia nem ocupada apenas pelo seu parceiro). Em <code>2P_3B*</code>, libertar na sua própria base inicial exige ainda ocupar pelo menos uma base inimiga; libertar diretamente em uma base inimiga que você já ocupa exige apenas <code>capturedPods &gt; 0</code>. Em <code>4P*</code>, libertar na sua base de canto ou na do seu parceiro exige ocupar pessoalmente uma casa OCTI neutra (<code>53</code>/<code>57</code>) ou inimiga (mesmo que o seu parceiro esteja eliminado, <code>xy*</code> na base de canto dele liberta o seu próprio Pod e nunca restaura um parceiro eliminado).",
            "<b>Conexão toroidal Sem bordas (<code>_ED</code>):</b> A coluna <code>1</code> conecta-se à coluna <code>9</code> e a linha <code>1</code> conecta-se à linha <code>9</code> (incluindo as diagonais) tanto para passos de 1 casa (<code>15g-95</code>) quanto para saltos de 2 casas (<code>97b-29x</code> saltando sobre <code>B18e</code>).",
            "<b>SuperProng e prelúdios bônus (<code>_SP</code>, <code>;</code>):</b> Um SuperProng dourado (<code>!</code>) é permanente após inserido. Antes da sua ação regular do turno, você pode executar <b>1 movimento prelúdio bônus para cada SuperProng que você controla no tabuleiro</b> (até <b>2</b> em <code>2P_SP</code> e até <b>4</b> em <code>4P_SP</code> quando SuperProng inimigos são capturados), separados por ponto e vírgula (ex.: <code>44ab!-66x;66ab!-67</code>). Cada prelúdio move 1 Pod estritamente em <b>linha reta</b> (passo de 1 casa ou cadeia colinear de saltos, sem mudar de direção) ao longo de um encaixe de SuperProng diferente que ainda não tenha executado um prelúdio bônus neste turno. As pilhas saltadas com <code>x</code> durante um prelúdio bônus de SuperProng são <b>removidas imediatamente</b> após esse prelúdio, antes de começar o próximo prelúdio de SuperProng ou a ação regular! Um Pod que se moveu em um prelúdio bônus ainda pode usar <b>qualquer um</b> dos seus Prong na fase regular do turno, e <b>a vitória só é verificada na conclusão do turno completo</b>.",
            "Figura 6: A pilha [A44a, A44beg] dividindo-se via 44a-45,44beg-34 ou desocupando totalmente 44 via 44a-45,44beg-66x-64x-44 para que A44beg pouse legalmente na casa desocupada 44 (Apêndice A.1).",
            "Figura 8: Tabuleiro Sem bordas (_ED) com passo toroidal 15g-95 e salto diagonal com captura 97b-29x através da borda conectada.",
            "Figura 9: Prelúdio bônus retilíneo de SuperProng dourado 44ab!-66x; (1 movimento bônus por SuperProng controlado no tabuleiro) removendo B55e! imediatamente antes do passo regular 66ab!-67, com vitória verificada na conclusão do turno completo.",
            "Capítulo 4 &mdash; Condições de vitória, eliminação, bloqueio e empate sem progresso",
            "<b>Família <code>2P_3B</code> (Padrão de 3 bases):</b> Vença na conclusão do seu turno completo se os seus Pod ocuparem simultaneamente <b>todas as 3</b> bases iniciais inimigas (<code>37</code>, <code>57</code> e <code>77</code> para A; <code>33</code>, <code>53</code> e <code>73</code> para B).",
            "<b>Família <code>2P_1B</code> (Blitz de 1 base):</b> Vença na conclusão do seu turno completo quando um Pod seu ocupar <b>qualquer uma</b> das 3 bases iniciais inimigas.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Vença na conclusão do seu turno completo quando um Pod seu ocupar <b>qualquer uma</b> das 4 bases iniciais inimigas (<code>26, 36, 46, 56</code> para A; <code>22, 32, 42, 52</code> para B).",
            "<b>Família de duplas <code>4P</code> (Dupla A&amp;C vs. Dupla B&amp;D):</b> Os parceiros diagonais vencem juntos na conclusão de um turno completo quando Pod aliados ocuparem simultaneamente <b>ambas as bases de canto inimigas</b> (<code>37</code> e <code>73</code> para a Dupla A&amp;C; <code>33</code> e <code>77</code> para a Dupla B&amp;D).",
            "<b>Eliminação permanente por ocupação de base em 4 jogadores e base segura do parceiro:</b> Em <code>4P</code>, se um Pod <b>inimigo</b> concluir o turno completo dele na sua base inicial de canto (ou se todos os seus Pod forem capturados sem restar nenhum Pod na reserva), você é <b>eliminado permanentemente</b> pelo restante da partida: todos os seus Pod restantes no tabuleiro passam para os seus Pod capturados (<code>capturedPods</code>), e <b>todos os Prong (normais e SuperProng) dos seus Pod recolhidos são transferidos para o jogador inimigo que ocupou a sua base</b>. Os assentos eliminados ficam fora da rotação de turnos enquanto o parceiro sobrevivente continua jogando. Por outro lado, <b>entrar com um passo ou salto na base inicial vazia do seu parceiro diagonal (ou já ocupada apenas por Pod controlados por você) é legal e não traz nenhuma consequência &mdash; isso nunca elimina o seu parceiro da partida</b> (se o seu parceiro ocupar temporariamente a sua base inicial enquanto você tiver zero Pod no tabuleiro, você apenas perde a vez até que ele a desocupe).",
            "<b>Captura total / Bloqueio:</b> Como reposicionar Prong é proibido, um jogador sem Pod na reserva e sem Prong na reserva cujos Pod no tabuleiro estejam totalmente encurralados fica com <b>zero lances legais</b>. Se o próximo jogador em <code>2P</code> / <code>OFK</code> (ou ambos os assentos inimigos em <code>4P</code>) tiver zero lances legais, o jogador ou dupla que acabou de jogar vence imediatamente (em <code>4P</code>, um jogador bloqueado perde a vez se o parceiro dele ainda tiver um lance legal). Partidas com relógio também terminam por tempo esgotado ou desistência.",
            "<b>Empate sem progresso em todas as variantes (<code>NO_PROGRESS_DRAW</code>, <code>noProgressPlies</code>):</b> Uma partida termina automaticamente em <b>empate (<code>NO_PROGRESS_DRAW</code>)</b> quando <b><code>30</code> lances consecutivos (<code>15</code> turnos completos por jogador)</b> em qualquer variante <code>2P*</code> ou <code>OFK</code> &mdash; ou <b><code>60</code> lances consecutivos (<code>15</code> rodadas completas)</b> em qualquer variante <code>4P*</code> &mdash; são concluídos sem nenhum <b>evento de progresso</b>: captura de Pod (<code>x</code>, incluindo prelúdios bônus em <code>_SP</code>), entrada de Pod da reserva (<code>xy</code>) ou libertação (<code>xy*</code>), inserção de Prong/SuperProng (<code>+d</code>/<code>+d!</code>) ou eliminação permanente de jogador em <code>4P</code>. Avaliado na conclusão do turno <b>após</b> as verificações de vitória por ocupação de bases, captura total e bloqueio sem lances legais (portanto, vencer no lance <code>30</code>/<code>60</code> continua contando como vitória).",
            "Figura 10: Parceria de 4 jogadores (4P): aliados diagonais A(33)&C(77) contra B(37)&D(73), pilha da mesma cor [A55ab, A55f], Pod do parceiro C65cf, entrada segura na base do parceiro e casas OCTI neutras 53 e 57.",
            "Figura 11: OCTI for Kids (OFK) na grade 6x7: 4 bases por lado, 0 Pod na reserva, Prong permanentes, sem empilhamento e vitória em 1 base (34a-36x).",
            "Figura 12: Estado de vitória em 2P_3B (status = O) com o Jogador A ocupando simultaneamente todas as 3 bases inimigas (A37c, A57a, A77g).",
            "Capítulo 5 &mdash; Matriz de referência das 13 variantes canônicas",
            "Toda partida na Web, Desktop, Android, Arquivo e SDK de robôs utiliza um dos seguintes <b>13 códigos canônicos de variante</b>:",
            "Código", "Grade", "Jogadores", "Pod (Início+Res.)", "Prong (N/SP)", "Modificadores e objetivo", "Ranqueada",
            "Sim", "Casual",
            "Regras padrão; ocupar todas as 3 bases inimigas",
            "SuperProng <code>!</code> + prelúdios retos <code>;</code> (&le;2); 3 bases",
            "Conexão toroidal Sem bordas; 3 bases",
            "SuperProng (&le;2 prelúdios) + Sem bordas; 3 bases",
            "Blitz em morte súbita; ocupar qualquer uma das bases inimigas",
            "SuperProng (&le;2 prelúdios) + morte súbita de 1 base",
            "Sem bordas + morte súbita de 1 base",
            "SuperProng (&le;2 prelúdios) + Sem bordas + morte súbita de 1 base",
            "Casas neutras <code>53/57</code>, eliminação permanente ao perder a base; 2 bases inimigas",
            "Duplas 4P + prelúdios bônus retos de SuperProng (&le;4)",
            "Duplas 4P + conexão toroidal Sem bordas",
            "Duplas 4P + SuperProng (&le;4 prelúdios) + Sem bordas",
            "OCTI for Kids: 4 bases por lado, 0 na reserva, sem pilhas nem lances compostos; basta 1 base para vencer",
            "Capítulo 6 &mdash; Notação oficial de lances e especificação OCTI-FEN",
            "Gramática algébrica da notação de lances",
            "Sintaxe", "Ação", "Exemplo",
            "Colocar Pod disponível da reserva em uma casa OCTI",
            "Libertar Pod próprio capturado em uma casa OCTI ocupada (somente <code>2P_3B*</code> e <code>4P*</code>)",
            "Inserir Prong normal no encaixe <code>d &isin; {a..h}</code>",
            "Inserir SuperProng dourado no encaixe <code>d</code>",
            "Passo de 1 casa sem captura",
            "Salto único ou cadeia de saltos múltiplos (<code>x</code> = captura opcional da pilha inteira)",
            "Lance composto dividindo Pod a partir da mesma casa de origem",
            "1&ndash;4 prelúdios bônus retilíneos de SuperProng (1 por SP no tabuleiro; remoção imediata com <code>x</code>) + turno regular",
            "Formato da string de posição OCTI-FEN",
            "Os estados do tabuleiro são serializados em strings <b>OCTI-FEN</b> separadas por dois-pontos (11 campos para <code>2</code> / <code>OFK</code>; 13 campos para <code>4</code>):",
            "<code>mode</code>: <code>2</code>, <code>4</code> ou <code>OFK</code> &bull; <code>bases</code>: <code>3</code> ou <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> ou <code>1</code>.",
            "<code>status</code>: <code>N</code> (não iniciada), <code>P</code> (em andamento), <code>O</code> (partida encerrada), <code>R</code> (desistência).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> ou <code>-</code> &bull; <code>elimMask</code>: máscara de bits de eliminação 4P (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (ex.: <code>4,0,25,0</code>).",
            "<code>squares</code>: lista de Pod separados por vírgulas em ordem por linhas <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (ex.: <code>A44ab!,B55e!</code>), ou <code>-</code> se vazio.",
            "<code>noProgressPlies</code> (12º/14º campo final opcional): lances consecutivos sem progresso (<code>0..1000</code>, padrão <code>0</code> quando omitido; <code>toPositionString()</code> emite a string canônica de 11/13 campos).",
            "Capítulo 7 &mdash; Pontuação Elo em dois grupos e regras competitivas da Arena",
            "As pontuações oficiais começam em <b>1200 Elo</b> em três rankings independentes 1 contra 1: <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b> e <b><code>OFK</code></b>. Todas as partidas de 4 jogadores (<code>4P*</code>), SuperProng (<code>*_SP</code>), Sem bordas (<code>*_ED</code>), combinadas (<code>*_SPED</code>) e offline são estritamente casuais (sem variação de Elo).",
            "<b>Grupo 1 &mdash; Rankings exclusivos para humanos (Classificação e Lista de jogadores):</b><br>Registra exclusivamente partidas online ranqueadas Humano contra Humano usando o fator Elo padrão <code>K = 32</code>. As pontuações exclusivas de humanos ficam 100% separadas das partidas com robôs.",
            "<b>Grupo 2 &mdash; Arena aberta de robôs e humanos (Classificação e Diretório de robôs):</b><br>Registra partidas online ranqueadas envolvendo pelo menos um robô de IA registrado (Humano contra Robô e Robô contra Robô) em uma escala competitiva unificada com três proteções contra manipulação:<ul><li><b>Partidas Humano contra Robô:</b> Jogo ranqueado sem limite de partidas com fator padrão <code>K = 32</code> (inclusive contra seus próprios robôs).</li><li><b>Fatores K em Robô contra Robô:</b> Confrontos entre proprietários diferentes usam o padrão <code>K = 32</code>; autotestes do mesmo proprietário usam fator reduzido <code>K = 16</code>.</li><li><b>Limite por par de robôs e proteção contra desistência precoce:</b> Confrontos Robô contra Robô são limitados a <b>4 partidas ranqueadas por janela móvel de 24 horas</b> entre qualquer par de robôs, independentemente da ordem (da 5ª partida em diante passa automaticamente a casual; não há limite diário contra robôs adversários diferentes), e desistências antes de <b>8 lances</b> registram variação de 0 Elo.</li></ul>"
        },
        /* 11. Korean (ko) */
        {
            "ko",
            {
                { "ch1", "제1장", "게임판 구조, Pod, Prong 및 초기 배치", "9x9 좌표계, 초기 예비석 및 8방향 나침반 소켓 a-h" },
                { "ch2", "제2장", "차례의 정규 행동", "Prong 삽입, 한 칸 이동, 연속 도약, 예비석 배치 및 해방" },
                { "ch3", "제3장", "스택, 복합 수 및 부록 A 예외 규칙", "쉼표 다중 Pod 복합 수, A.1 루프 복귀, 경계 없음 및 SuperProng" },
                { "ch4", "제4장", "승리 조건 및 4P 탈락", "3베이스 대 1베이스 점령 승리, 4P 영구 탈락 및 30/60수 무진전 무승부" },
                { "ch5", "제5장", "공식 13종 변형 일람표", "게임판 크기, Pod/Prong 배정, 특수 규칙 및 레이팅 적용 여부" },
                { "ch6", "제6장", "공식 기보 표기법 및 OCTI-FEN 문법", "공식 대수 기보 문법과 11/13필드 OCTI-FEN 형식" },
                { "ch7", "제7장", "이중 풀 Elo 및 아레나 규정", "인간 전용 풀 1과 오픈 로봇 및 인간 아레나 풀 2 안전장치" }
            },
            "OCTI 공식 도해 규칙서",
            "다중 변형 통합 규칙, 전술 메커니즘, 부록 A 예외 사례, 공식 기보/OCTI-FEN 문법 및 이중 풀 Elo 경쟁 규정.",
            "목차:",
            "제1장 &mdash; 게임판 구조, Pod, Prong 및 초기 배치",
            "OCTI는 이동 벡터를 직접 조립해 나가는 결정론적 완전 정보 추상 전략 게임입니다. 모든 Pod는 빈 상태(이동 방향 0개)로 게임판에 놓이며, 플레이어가 매 차례 하나씩 <b>Prong</b>을 삽입하면서 방향 기동력을 얻습니다.",
            "Pod(둘레에 8방향 소켓이 있는 팔각형 메인 기물)",
            "Prong(이동 방향을 부여하기 위해 Pod 소켓에 꽂는 방향 핀)",
            "SuperProng(정규 차례 전 직선 보너스 이동을 제공하는 금색 특수 핀)",
            "<b>9&times;9 표준 격자:</b> 열 <code>x = 1..9</code>는 서쪽에서 동쪽(왼쪽에서 오른쪽)으로, 행 <code>y = 1..9</code>는 남쪽에서 북쪽(아래에서 위)으로 이어집니다. 모든 칸은 두 자리 숫자 <code>xy</code>로 표기합니다(예: <code>53</code>은 5열 3행).",
            "<b>2인용 초기 자원(<code>2P_3B</code> &amp; <code>2P_1B</code>):</b> 플레이어 A는 OCTI 홈 베이스 <code>33</code>, <code>53</code>, <code>73</code>에 <b>Pod 3개</b>를 두고 시작하며, 플레이어 B는 OCTI 홈 베이스 <code>37</code>, <code>57</code>, <code>77</code>에 <b>Pod 3개</b>를 두고 시작합니다. 또한 각 플레이어는 <b>예비석 Pod 4개</b>(진영당 총 Pod 7개)와 <b>Prong 25개</b>(<code>_SP</code> 변형에서는 <b>일반 Prong 24개 + 금색 SuperProng 1개</b>)를 보유합니다.",
            "<b>8방향 나침반 Prong 소켓(<code>a</code>&ndash;<code>h</code>):</b> 모든 팔각형 Pod 둘레에는 북쪽부터 시계 방향으로 대응되는 8개의 소켓이 있습니다: <code>a</code>=북쪽(<code>0,+1</code>), <code>b</code>=북동쪽(<code>+1,+1</code>), <code>c</code>=동쪽(<code>+1,0</code>), <code>d</code>=남동쪽(<code>+1,-1</code>), <code>e</code>=남쪽(<code>0,-1</code>), <code>f</code>=남서쪽(<code>-1,-1</code>), <code>g</code>=서쪽(<code>-1,0</code>), <code>h</code>=북서쪽(<code>-1,+1</code>).",
            "그림 1: 홈 베이스 행 33/53/73 대 37/57/77을 갖춘 표준 9x9 2인용 초기 배치(2P_3B).",
            "그림 2: 2P_1B(1베이스 속공전)는 동일한 초기 배치를 사용하지만 적 베이스 중 어느 1곳이라도 점령하면(55ab-57x) 승리합니다.",
            "그림 3: Pod A55acg 둘레의 8방향 나침반 소켓 a-h와 빈 둘레 소켓에 보유 Prong을 삽입(55acg+b)하는 모습.",
            "제2장 &mdash; 차례의 정규 행동",
            "플레이어 A가 먼저 두며 차례는 엄격히 교대됩니다(<code>4P</code>에서는 탈락한 자리를 건너뛰며 <code>A &rarr; B &rarr; C &rarr; D</code> 순서로 순환합니다). 차례 넘기기(패스)는 금지되며, 자신의 차례에는 다음 정규 행동 중 <b>정확히 하나</b>를 실행해야 합니다:",
            "<b>Prong 삽입(<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> 보유 중인 여분 Prong 1개를 가져와 게임판 위 자신의 Pod 중 빈 소켓에 꽂습니다(예: <code>55acg+b</code>, 금색 SuperProng은 <code>33+b!</code>). 한 번 삽입한 Prong은 해당 Pod에 영구 고정되며, 어떤 변형에서도 이미 장착된 Prong을 다른 소켓으로 옮겨 꽂는 것은 허용되지 않습니다.",
            "<b>한 칸 이동(<code>xy&lt;cfg&gt;-x'y'</code>):</b> Pod에 장착된 Prong 방향 중 하나를 따라 빈 칸이나 자신의 Pod만으로 이루어진 스택 위로 1칸 이동합니다(예: <code>44ab-45</code>). 한 칸 이동은 <b>절대 기물을 포획하지 않으며</b> 다른 플레이어의 Pod가 있는 칸(<code>4P</code>의 대각선 파트너 포함)으로는 들어갈 수 없습니다.",
            "<b>2칸 도약 및 연속 도약(<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> 장착된 Prong 방향을 따라 인접한 기물(아군 또는 적군)이 있는 칸을 뛰어넘어 그 너머의 빈 칸으로 2칸 도약합니다. 같은 Pod에 장착된 어느 Prong 방향이든 이용해 추가 도약을 연속으로 이어갈 수 있으며(도약 중 방향 전환 가능), 합법적인 도약 후 언제든 도중에 멈출 수 있습니다.",
            "<b>예비석 Pod 배치(<code>xy</code>) 또는 포획된 Pod 해방(<code>xy*</code>):</b> 자신의 홈 베이스가 비어 있거나 자신의 Pod만 놓여 있을 때 그 홈 베이스(또는 이미 자신이 점령 중인 적 베이스)에 예비석 Pod 1개를 배치하거나, 3베이스(<code>2P_3B*</code>) 및 4인용(<code>4P*</code>) 대국에서 자격 조건 충족 시 포획된 자신의 Pod 1개를 이미 자신이 점유 중인 OCTI 칸에 해방(<code>xy*</code>)합니다(1베이스 서든데스 변형 및 <code>OFK</code>에는 해방이 적용되지 않습니다).",
            "<b>선택적 스택 전체 포획 및 Prong 노획:</b> 기물이 있는 칸을 뛰어넘을 때 포획 여부는 <b>선택 사항</b>입니다. 뒤에 <code>x</code>를 붙이면(예: <code>44ab-66x</code>) <b>뛰어넘은 스택의 모든 Pod를 포획</b>하며(스택 전체 포획만 허용되며 일부만 골라 잡는 것은 불가), <code>x</code>를 생략하면(예: <code>44ab-66</code>) 뛰어넘은 스택을 그대로 남겨 둡니다. 정규 단일 수 또는 복합 수 도중 포획된 스택은 차례가 끝날 때 각 Pod 소유자의 포획된 Pod(<code>capturedPods</code>) 풀로 옮겨지며, <b>그 Pod에 꽂혀 있던 모든 Prong은 포획한 플레이어의 보유 Prong 공급처로 귀속됩니다</b>! 아군/동맹 Pod는 포획 없이 자유롭게 뛰어넘을 수 있습니다.",
            "그림 4: Pod A44ab가 북쪽으로 한 칸 이동(44ab-45)하는 경우와 북동쪽으로 B55e를 뛰어넘어 선택적 스택 전체 포획(44ab-66x)을 수행하는 2칸 도약 비교.",
            "그림 5: 아군 A44h를 뛰어넘은 뒤 북쪽으로 방향을 틀어 B56e를 포획하고, 다시 남동쪽으로 틀어 B66c를 포획하는 연속 도약 33abd-55-57x-75x.",
            "그림 7: 빈 홈 베이스 33에 예비석 Pod를 배치하는 행동과, 2P_3B* 및 4P*에서 적(또는 4P 중립) OCTI 칸을 점령한 뒤 포획된 자신의 Pod를 해방(53* 또는 57*)하는 행동.",
            "제3장 &mdash; 스택, 복합 수 및 부록 A 예외 규칙",
            "<b>자신의 Pod 스택 및 다중 Pod 복합 수(<code>,</code>):</b> 본인 색상의 Pod 여러 개는 같은 칸에 함께 쌓일 수 있습니다(<code>OFK</code>에서는 금지; <code>4P</code>에서도 대각선 파트너를 포함한 다른 색상의 Pod와 함께 쌓는 것은 절대 허용되지 않습니다). <b>차례 시작 시 같은 칸에 있던</b> 자신의 Pod 중 원하는 일부 또는 전부는 한 차례 안에서 쉼표로 연결하여 함께 이동하거나 서로 독립된 한 칸 이동/도약 경로로 분리되어 이동할 수 있습니다(예: <code>44a-45,44beg-34</code>). Pod 배치와 Prong 삽입은 이동과 결합할 수 없습니다.",
            "<b>스택 전체 포획 및 차례 종료 시 일괄 제거:</b> <code>x</code>를 붙여 기물이 있는 칸을 뛰어넘으면 <b>그 칸의 스택에 속한 모든 Pod</b>를 포획합니다. 정규 단일 수 및 복합 수에서는 <code>x</code>로 뛰어넘은 스택이 같은 차례 동안 남은 복합 수 스택 동료들이 디딤돌로 삼아 뛰어넘을 수 있도록 게임판 위에 남아 있다가 차례가 끝날 때 한꺼번에 제거됩니다. 하나의 Pod가 한 번의 연속 도약 중에 <b>정확히 같은 칸을 두 번 뛰어넘는 것은 절대 불가능</b>합니다.",
            "<b>부록 A.1 &mdash; 출발 칸 비우기, 순환 복귀 도약 및 동반 스택 기물 도약 금지:</b> 도약 연쇄가 이어지는 동안 도약하는 Pod는 <b>자신의 출발 칸 <code>S</code>나 차례 시작 시 자신과 함께 쌓여 있던 어떤 Pod도 뛰어넘을 수 없습니다</b>(따라서 경계 없음 <code>_ED</code> 순환 게임판에서 <code>44</code>에 가만히 남아 있는 <code>A44a</code>를 뛰어넘는 <code>44bcd-66-86-16-34-54</code> 같은 수는 불법입니다). 또한 연속 도약 루프 끝에 <b>출발 칸 <code>S</code>에 다시 착지</b>하는 것은 <b>차례 시작 시 <code>S</code>에 있던 모든 Pod가 이번 수 동안 <code>S</code>를 완전히 비웠을 때만</b> 허용됩니다(예: 복합 수로 출발 칸을 모두 비우는 <code>44a-45,44beg-66x-64x-44</code>는 <code>44</code>가 완전히 비워지므로 합법이지만, <code>44</code>에 가만히 있는 <code>A44a</code>를 남겨 두는 단일 이동 <code>44beg-66x-64x-44</code>는 불법입니다). 포획 없는 순환 연속 도약(<code>S</code>가 완전히 비워졌을 때 <code>S &rarr; ... &rarr; S</code>) 역시 합법이며 단순 이동 수로 집계됩니다(무진전 무승부 기준에 <code>+1</code> 가산).",
            "<b>Pod 해방 적용 범위 및 자격 조건(<code>2P_3B*</code> &amp; <code>4P*</code>):</b> 포획된 Pod 해방(<code>xy*</code>)은 오직 <b>3베이스(<code>2P_3B*</code>) 및 4인용(<code>4P*</code>)</b> 대국에만 적용되며(1베이스 서든데스 및 <code>OFK</code>에서는 불가), 항상 <b>자신의</b> 포획된 Pod 1개(<code>capturedPods &gt; 0</code>)를 해방하고, 대상 OCTI 칸에 <b>이미 자신의 Pod가 최소 1개 이상 자리하고 있어야만</b> 합니다(빈 칸이나 파트너 기물만 있는 칸에는 불가). <code>2P_3B*</code>에서 자신의 홈 베이스로 해방하려면 적 베이스를 최소 1곳 이상 점령하고 있어야 하며, 이미 자신이 점령 중인 적 베이스에 직접 해방할 때는 <code>capturedPods &gt; 0</code>만 충족하면 됩니다. <code>4P*</code>에서 자신이나 파트너의 코너 베이스로 해방하려면 본인이 직접 중립(<code>53</code>/<code>57</code>) 또는 적 OCTI 칸을 점령하고 있어야 합니다(파트너가 탈락했더라도 파트너 코너 베이스에서의 <code>xy*</code> 행동은 자신의 Pod를 해방할 뿐 탈락한 파트너를 부활시키지 않습니다).",
            "<b>경계 없음 토러스 순환(<code>_ED</code>):</b> 한 칸 이동(<code>15g-95</code>)과 2칸 도약(<code>B18e</code>를 뛰어넘는 <code>97b-29x</code>) 모두에서 <code>1</code>열과 <code>9</code>열, <code>1</code>행과 <code>9</code>행이(대각선 포함) 경계 없이 반대편으로 연결됩니다.",
            "<b>SuperProng 및 보너스 선제 이동(<code>_SP</code>, <code>;</code>):</b> 금색 SuperProng(<code>!</code>)은 한 번 삽입하면 영구 고정됩니다. 정규 차례 행동을 수행하기 전에, 세미콜론으로 구분하여 <b>게임판 위에서 자신이 제어하는 SuperProng 1개당 보너스 선제 이동 1회</b>(적 SuperProng 포획 시 <code>2P_SP</code>에서는 최대 <b>2회</b>, <code>4P_SP</code>에서는 최대 <b>4회</b>; 예: <code>44ab!-66x;66ab!-67</code>)를 실행할 수 있습니다. 각 선제 이동은 이번 차례에 아직 보너스 선제 이동에 쓰지 않은 서로 다른 SuperProng 소켓의 방향을 따라 Pod 1개를 오직 <b>일직선</b>(한 칸 이동 또는 방향 전환 없는 일직선 연속 도약)으로만 이동시킵니다. SuperProng 보너스 선제 이동 중 <code>x</code>로 뛰어넘은 스택은 다음 SuperProng 선제 이동이나 정규 행동이 시작되기 전에 <b>즉시 제거</b>됩니다! 보너스 선제 이동을 마친 Pod도 정규 차례 단계에서 자신의 <b>모든</b> Prong을 사용할 수 있으며, <b>승리 판정은 전체 차례가 완전히 끝났을 때만 확인합니다</b>.",
            "그림 6: 스택 [A44a, A44beg]가 44a-45,44beg-34로 분리 이동하거나, 44a-45,44beg-66x-64x-44로 44를 완전히 비워 A44beg가 비워진 44에 합법적으로 복귀 착지하는 모습(부록 A.1).",
            "그림 8: 경계 없음(_ED) 순환 한 칸 이동 15g-95 및 포털 경계를 넘어가는 대각선 순환 도약 포획 97b-29x.",
            "그림 9: 금색 SuperProng 직선 보너스 선제 이동 44ab!-66x;(게임판 위 제어 SuperProng당 보너스 이동 1회)로 정규 한 칸 이동 66ab!-67 직전에 포획된 B55e!를 즉시 제거하는 모습. 승리 여부는 전체 차례 완료 시 확인합니다.",
            "제4장 &mdash; 승리, 탈락, 봉쇄 및 무진전 무승부 조건",
            "<b><code>2P_3B</code> 계열(3베이스 표준전):</b> 전체 차례를 마쳤을 때 자신의 Pod들이 적 홈 베이스 <b>3곳 모두</b>(A는 <code>37</code>, <code>57</code>, <code>77</code>; B는 <code>33</code>, <code>53</code>, <code>73</code>)를 동시에 점령하고 있으면 승리합니다.",
            "<b><code>2P_1B</code> 계열(1베이스 속공전):</b> 전체 차례를 마쳤을 때 자신의 Pod가 적 홈 베이스 3곳 중 <b>어느 1곳이라도</b> 점령하고 있으면 승리합니다.",
            "<b><code>OFK</code>(OCTI for Kids, 6&times;7):</b> 전체 차례를 마쳤을 때 자신의 Pod가 적 홈 베이스 4곳(A는 <code>26, 36, 46, 56</code>; B는 <code>22, 32, 42, 52</code>) 중 <b>어느 1곳이라도</b> 점령하고 있으면 승리합니다.",
            "<b><code>4P</code> 파트너십 계열(팀 A&amp;C 대 팀 B&amp;D):</b> 전체 차례를 마쳤을 때 동맹 Pod들이 <b>적 코너 베이스 두 곳 모두</b>(팀 A&amp;C는 <code>37</code> &amp; <code>73</code>; 팀 B&amp;D는 <code>33</code> &amp; <code>77</code>)를 동시에 점령하고 있으면 대각선 파트너가 함께 승리합니다.",
            "<b>4인용 베이스 점령 영구 탈락 및 안전한 파트너 베이스:</b> <code>4P</code>에서 <b>적</b> Pod가 어떤 플레이어의 코너 홈 베이스 위에서 전체 차례를 마치면(또는 그 플레이어의 모든 Pod가 포획되고 남은 예비석 Pod도 0개이면), 그 플레이어는 남은 대국 동안 <b>영구 탈락</b>합니다. 그 플레이어가 게임판 위에 남긴 모든 Pod는 그 플레이어의 포획된 Pod(<code>capturedPods</code>) 풀로 수거되며, <b>수거된 Pod에 꽂혀 있던 모든 Prong(일반 Prong 및 SuperProng)은 베이스를 점령한 적 플레이어에게 이전됩니다</b>. 탈락한 자리는 차례 순환에서 건너뛰며 생존한 파트너가 계속 대국을 이어갑니다. 반대로, <b>대각선 파트너의 비어 있는(또는 이동하는 플레이어 본인의 Pod만 이미 놓여 있는) 시작 베이스 위로 한 칸 이동하거나 도약해 들어가는 것은 합법이며 아무런 불이익이 없습니다&mdash;파트너를 절대 대국에서 탈락시키지 않습니다</b>(어떤 플레이어의 게임판 위 Pod가 0개일 때 파트너가 그 플레이어의 시작 베이스를 일시적으로 점유하고 있으면, 파트너가 그 베이스를 비울 때까지 해당 자리의 차례만 건너뜁니다).",
            "<b>전멸 / 봉쇄:</b> Prong 재배치가 금지되어 있으므로, 예비석 Pod와 보유 Prong이 모두 0개이고 게임판 위 Pod들이 완전히 갇힌 플레이어는 <b>합법적인 수가 0개</b>가 됩니다. <code>2P</code> / <code>OFK</code>에서 다음 차례 플레이어(또는 <code>4P</code>에서 적 두 자리 모두)에게 합법적인 수가 하나도 없으면 직전에 수를 둔 플레이어/팀이 즉시 승리합니다(<code>4P</code>에서 한 플레이어가 봉쇄되더라도 파트너에게 합법적인 수가 남아 있으면 해당 자리의 차례만 건너뜁니다). 시간 제어 대국은 시간 초과 또는 기권으로도 종료됩니다.",
            "<b>공통 무진전 무승부(<code>NO_PROGRESS_DRAW</code>, <code>noProgressPlies</code>):</b> 모든 <code>2P*</code> 또는 <code>OFK</code> 변형에서 <b>연속 <code>30</code>수(플레이어당 <code>15</code>턴)</b>, 또는 모든 <code>4P*</code> 변형에서 <b>연속 <code>60</code>수(<code>15</code>라운드)</b> 동안 Pod 포획(<code>x</code>, <code>_SP</code> 보너스 선제 이동 포함), 예비석 Pod 배치(<code>xy</code>) 또는 해방(<code>xy*</code>), Prong/SuperProng 삽입(<code>+d</code>/<code>+d!</code>), <code>4P</code> 플레이어 영구 탈락 등 어떠한 <b>진전 이벤트</b>도 없이 완료되면 대국은 자동으로 <b>무승부(<code>NO_PROGRESS_DRAW</code>)</b>로 종료됩니다. 이 판정은 차례 완료 시 베이스 점령 승리, 전부 포획 전멸, 합법적인 수가 없는 봉쇄 판정 <b>이후에</b> 이루어지므로 <code>30</code>/<code>60</code>수째에 승리 조건을 달성하면 승리가 인정됩니다.",
            "그림 10: 4인 파트너십(4P): 대각선 동맹 A(33)&C(77) 대 B(37)&D(73), 같은 색 스택 [A55ab, A55f], 파트너 Pod C65cf, 안전한 파트너 베이스 진입, 중립 베이스 53/57.",
            "그림 11: 6x7 격자에서 진행되는 OCTI for Kids(OFK): 진영당 베이스 4곳, 예비석 Pod 0개, 영구 고정 Prong, 스택 금지 및 1베이스 승리(34a-36x).",
            "그림 12: 플레이어 A가 적 베이스 3곳(A37c, A57a, A77g)을 동시에 점령한 2P_3B 승리 국면(status = O).",
            "제5장 &mdash; 공식 13종 변형 일람표",
            "웹, 데스크톱, Android, 아카이브 및 봇 SDK의 모든 대국은 다음 <b>13개의 공식 변형 코드</b> 중 하나를 사용합니다:",
            "코드", "격자", "인원", "Pod (시작+예비석)", "Prong (일반/SP)", "특수 규칙 및 승리 목표", "레이팅",
            "적용", "친선",
            "표준 규칙; 적 홈 베이스 3곳 모두 점령",
            "SuperProng <code>!</code> + <code>;</code> 직선 보너스 선제 이동(&le;2회); 3베이스",
            "경계 없음 토러스 순환; 3베이스",
            "SuperProng(선제 이동 &le;2회) + 경계 없음; 3베이스",
            "서든데스 속공전; 적 베이스 중 어느 1곳 점령",
            "SuperProng(선제 이동 &le;2회) + 1베이스 서든데스",
            "경계 없음 + 1베이스 서든데스",
            "SuperProng(선제 이동 &le;2회) + 경계 없음 + 1베이스",
            "중립 <code>53/57</code>, 베이스 점령 시 영구 탈락; 적 코너 베이스 2곳",
            "4P 파트너십 + SuperProng 직선 보너스 선제 이동(&le;4회)",
            "4P 파트너십 + 경계 없음 토러스 순환",
            "4P 파트너십 + SuperProng(선제 이동 &le;4회) + 경계 없음",
            "진영당 베이스 4곳, 예비석 0개, 스택 불가, 복합 수 불가; 1베이스 승리",
            "제6장 &mdash; 공식 기보 표기법 및 OCTI-FEN 명세",
            "대수 기보 표기 문법",
            "구문", "행동", "예시",
            "OCTI 칸에 예비석 Pod 배치",
            "점유 중인 OCTI 칸에 포획된 자신의 Pod 해방(<code>2P_3B*</code> 및 <code>4P*</code> 전용)",
            "소켓 <code>d &isin; {a..h}</code>에 일반 Prong 삽입",
            "소켓 <code>d</code>에 금색 SuperProng 삽입",
            "포획 없는 한 칸 이동",
            "단일 또는 연속 도약(<code>x</code> = 선택적 스택 전체 포획)",
            "같은 출발 칸의 Pod들을 나누어 움직이는 복합 수",
            "1&ndash;4회의 직선 SuperProng 보너스 선제 이동(게임판 위 SP당 1회; <code>x</code> 포획 즉시 제거) + 정규 차례",
            "OCTI-FEN 국면 문자열 형식",
            "게임판 상태는 콜론으로 구분된 <b>OCTI-FEN</b> 문자열로 직렬화됩니다(<code>2</code> / <code>OFK</code>는 11개 필드, <code>4</code>는 13개 필드):",
            "<code>mode</code>: <code>2</code>, <code>4</code> 또는 <code>OFK</code> &bull; <code>bases</code>: <code>3</code> 또는 <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> 또는 <code>1</code>.",
            "<code>status</code>: <code>N</code>(시작 전), <code>P</code>(진행 중), <code>O</code>(대국 종료), <code>R</code>(기권).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> 또는 <code>-</code> &bull; <code>elimMask</code>: 4P 탈락 비트마스크(<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (예: <code>4,0,25,0</code>).",
            "<code>squares</code>: 행 우선 순서로 쉼표 구분된 Pod 목록 <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (예: <code>A44ab!,B55e!</code>), 비어 있으면 <code>-</code>.",
            "<code>noProgressPlies</code>(선택적 12번째/14번째 마지막 필드): 연속 무진전 수(<code>0..1000</code>, 생략 시 기본값 <code>0</code>; 표준 <code>toPositionString()</code> 호출은 11/13필드 문자열을 출력).",
            "제7장 &mdash; 이중 풀 Elo 및 경쟁 아레나 규정",
            "공식 레이팅은 세 가지 독립적인 1대1 변형 래더인 <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b>, <b><code>OFK</code></b>에서 <b>1200 Elo</b>로 시작합니다. 모든 4인용(<code>4P*</code>), SuperProng(<code>*_SP</code>), 경계 없음(<code>*_ED</code>), 혼합 변형(<code>*_SPED</code>) 및 오프라인 대국은 친선(비레이팅) 대국입니다.",
            "<b>풀 1 &mdash; 인간 전용 래더(리더보드 및 플레이어 명단):</b><br>표준 Elo 계수 <code>K = 32</code>를 사용하여 온라인 레이팅 인간 대 인간 대국만을 집계하며, 로봇 대국 결과는 전혀 반영되지 않습니다.",
            "<b>풀 2 &mdash; 오픈 로봇 &amp; 인간 아레나(리더보드 및 로봇 디렉터리):</b><br>등록된 AI 로봇이 최소 1기 이상 참여하는 온라인 레이팅 대국(인간 대 로봇 및 로봇 대 로봇)을 3가지 어뷰징 방지 안전장치로 집계합니다:<ul><li><b>인간 대 로봇 대국:</b> 본인이 소유한 로봇과의 대국을 포함해 표준 <code>K = 32</code>로 횟수 제한 없이 레이팅 대국이 가능합니다.</li><li><b>로봇 대 로봇 K-계수:</b> 서로 다른 소유자 간 대국은 표준 <code>K = 32</code>, 동일 소유자의 자체 테스트 대국은 축소된 <code>K = 16</code>을 적용합니다.</li><li><b>로봇 대 로봇 쌍별 상한 및 조기 기권 방지:</b> 순서 무관 동일 로봇 쌍 간의 로봇 대 로봇 대국은 <b>최근 24시간당 레이팅 4국</b>까지만 반영되며(5국째부터는 자동으로 친선 전환; 서로 다른 상대 로봇과의 일일 상한 없음), <b>8수 미만</b> 조기 기권 시 Elo 변동은 0입니다.</li></ul>"
        },
        /* 12. Polish (pl) */
        {
            "pl",
            {
                { "ch1", "Rozdział 1", "Geometria planszy, Pod, Prong i ustawienie początkowe", "Współrzędne 9×9, rezerwy początkowe i 8 gniazd kierunkowych a–h" },
                { "ch2", "Rozdział 2", "Akcje w turze", "Wstawienie Prong, krok o 1 pole, łańcuch skoków, rezerwa i wyzwolenie" },
                { "ch3", "Rozdział 3", "Stosy, ruchy złożone i Dodatek A", "Ruchy złożone z przecinkiem, pętle A.1, Bez krawędzi i SuperProng" },
                { "ch4", "Rozdział 4", "Zwycięstwo i eliminacja w 4P", "Zajęcie 3 lub 1 bazy, eliminacja w 4P i remis bez postępu po 30/60 ruchach" },
                { "ch5", "Rozdział 5", "Macierz 13 kanonicznych wariantów", "Wymiary planszy, zasoby Pod i Prong, modyfikatory oraz status rankingowy" },
                { "ch6", "Rozdział 6", "Notacja ruchów i gramatyka OCTI-FEN", "Oficjalna notacja algebraiczna i format OCTI-FEN z 11/13 elementów" },
                { "ch7", "Rozdział 7", "Dwupulowy system Elo i zasady Areny", "Pula 1 tylko dla ludzi kontra Pula 2 (Otwarta Arena Robotów i Ludzi) i jej zabezpieczenia" }
            },
            "Oficjalna ilustrowana Księga zasad OCTI",
            "Pełne zasady wszystkich wariantów, mechaniki taktyczne, przypadki brzegowe Dodatku A, oficjalna gramatyka ruchów i OCTI-FEN oraz regulamin rywalizacji w dwupulowym systemie Elo.",
            "Rozdziały:",
            "Rozdział 1 &mdash; Geometria planszy, Pod, Prong i ustawienie początkowe",
            "OCTI to deterministyczna abstrakcyjna gra strategiczna z pełną informacją, oparta na budowaniu wektorów ruchu. Każdy Pod wchodzi na planszę pusty (bez żadnego kierunku ruchu) i zyskuje kierunkową mobilność w miarę jak gracze w kolejnych turach wstawiają po jednym kołku <b>Prong</b>.",
            "Pod (ośmiokątny główny pion z 8 gniazdami kierunkowymi)",
            "Prong (kołek kierunkowy wkładany do gniazda Pod, aby odblokować ruch)",
            "SuperProng (złoty kołek dający bonusowy ruch wstępny w linii prostej)",
            "<b>Standardowa siatka 9&times;9:</b> Kolumny <code>x = 1..9</code> biegną z zachodu na wschód (od lewej do prawej); rzędy <code>y = 1..9</code> biegną z południa na północ (od dołu do góry). Każde pole zapisuje się dwiema cyframi <code>xy</code> (na przykład <code>53</code> to kolumna 5, rząd 3).",
            "<b>Zasoby początkowe dla 2 graczy (<code>2P_3B</code> i <code>2P_1B</code>):</b> Gracz A zaczyna z <b>3 Pod</b> na bazach domowych OCTI <code>33</code>, <code>53</code> i <code>73</code>; Gracz B zaczyna z <b>3 Pod</b> na bazach domowych OCTI <code>37</code>, <code>57</code> i <code>77</code>. Każda ze stron dysponuje ponadto <b>4 rezerwowymi Pod</b> (łącznie po 7 Pod na gracza) oraz <b>25 Prong</b> (lub <b>24 zwykłymi Prong + 1 złotym SuperProng</b> w wariantach <code>_SP</code>).",
            "<b>8 kierunkowych gniazd Prong (<code>a</code>&ndash;<code>h</code>):</b> Każdy ośmiokątny Pod ma na obwodzie 8 gniazd oznaczonych zgodnie z ruchem wskazówek zegara od północy: <code>a</code>=N (<code>0,+1</code>), <code>b</code>=NE (<code>+1,+1</code>), <code>c</code>=E (<code>+1,0</code>), <code>d</code>=SE (<code>+1,-1</code>), <code>e</code>=S (<code>0,-1</code>), <code>f</code>=SW (<code>-1,-1</code>), <code>g</code>=W (<code>-1,0</code>) oraz <code>h</code>=NW (<code>-1,+1</code>).",
            "Rysunek 1: Standardowe ustawienie 9×9 dla 2 graczy (2P_3B) z rzędami baz domowych 33/53/73 kontra 37/57/77.",
            "Rysunek 2: Wariant 2P_1B (blitz o 1 bazę) korzysta z identycznego ustawienia, lecz zajęcie którejkolwiek z baz przeciwnika (55ab-57x) wystarcza do zwycięstwa.",
            "Rysunek 3: 8 kierunkowych gniazd a–h wokół piona Pod A55acg oraz wstawienie Prong z rezerwy (55acg+b) do pustego gniazda na obwodzie.",
            "Rozdział 2 &mdash; Akcje w turze",
            "Gracz A wykonuje pierwszy ruch, a tury następują ściśle naprzemiennie (w <code>4P</code> kolejność tur przebiega cyklicznie <code>A &rarr; B &rarr; C &rarr; D</code> z pominięciem wyeliminowanych miejsc). Pasowanie jest zabronione; w swojej turze musisz wykonać <b>dokładnie jedną</b> regularną akcję tury:",
            "<b>Wstawienie Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Weź 1 Prong ze swojej wolnej rezerwy i wstaw go do pustego gniazda jednego ze swoich Pod na planszy (np. <code>55acg+b</code> albo <code>33+b!</code> dla złotego SuperProng). Raz wstawiony Prong pozostaje w tym Pod na stałe &mdash; przekładanie zamontowanego Prong do innego gniazda nie jest dozwolone w żadnym wariancie.",
            "<b>Krok o 1 pole (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Przesuń Pod o 1 pole wzdłuż jednego z zamontowanych w nim kierunków Prong na puste pole lub stos złożony z twoich własnych Pod (np. <code>44ab-45</code>). Krok o 1 pole <b>nigdy nie zbija</b> i nie pozwala wejść na pole zajęte przez Pod innego gracza (w tym twojego partnera po przekątnej w <code>4P</code>).",
            "<b>Skok o 2 pola i skok wielokrotny (<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> Przeskocz o 2 pola wzdłuż zamontowanego kierunku Prong nad sąsiednim zajętym polem (własnym, sojuszniczym lub wrogim) na puste pole bezpośrednio za nim. Tym samym Pod możesz łączyć kolejne skoki wzdłuż dowolnych zamontowanych w nim Prong (zmieniając kierunek między skokami) oraz wcześniej zakończyć serię po dowolnym legalnym skoku.",
            "<b>Wystawienie rezerwowego Pod (<code>xy</code>) lub wyzwolenie zbitego Pod (<code>xy*</code>):</b> Umieść 1 rezerwowy Pod na własnej bazie domowej, gdy jest pusta lub zajęta wyłącznie przez twoje własne Pod (albo na bazie przeciwnika, którą już zajmujesz), lub &mdash; w partiach o 3 bazy (<code>2P_3B*</code>) oraz dla 4 graczy (<code>4P*</code>) &mdash; wyzwól 1 ze swoich własnych zbitych Pod (<code>xy*</code>) na już zajmowane przez ciebie pole OCTI po spełnieniu warunków (wyzwolenie nie obowiązuje w wariantach nagłej śmierci o 1 bazę ani w <code>OFK</code>).",
            "<b>Opcjonalne zbicie całego stosu i przejęcie Prong:</b> Przeskoczenie zajętego pola sprawia, że zbicie każdego przeskoczonego stosu jest <b>dobrowolne</b>: dopisz <code>x</code> (np. <code>44ab-66x</code>), aby zbić <b>wszystkie Pod w przeskoczonym stosie</b> (zbicie całego stosu &mdash; częściowe ani wybiórcze zbicie nie jest dozwolone), albo pomiń <code>x</code> (np. <code>44ab-66</code>), aby zostawić przeskoczony stos na planszy. Podczas zwykłych ruchów pojedynczych i złożonych zbite stosy są zdejmowane na końcu tury do puli zbitych Pod (<code>capturedPods</code>) właściciela każdego Pod, natomiast <b>wszystkie zamontowane w nich Prong przechodzą do wolnej rezerwy Prong gracza zbijającego</b>. Własne i sojusznicze piony Pod można swobodnie przeskakiwać bez zbijania.",
            "Rysunek 4: Pod A44ab wykonujący krok o 1 pole na północ (44ab-45) kontra skok o 2 pola na NE nad B55e z opcjonalnym zbiciem całego stosu (44ab-66x).",
            "Rysunek 5: Skok wielokrotny 33abd-55-57x-75x: przeskok nad własnym A44h, zwrot na północ ze zbiciem B56e, a następnie na SE ze zbiciem B66c.",
            "Rysunek 7: Wystawienie rezerwowego Pod na pustą bazę domową 33 kontra wyzwolenie własnego zbitego Pod (53* lub 57*) w 2P_3B* i 4P* po zajęciu wrogiego (lub neutralnego w 4P) pola OCTI.",
            "Rozdział 3 &mdash; Stosy, ruchy złożone i przypadki brzegowe Dodatku A",
            "<b>Tworzenie stosów własnych Pod i ruchy złożone wieloma Pod (<code>,</code>):</b> Wiele Pod twojego własnego koloru może tworzyć stos na tym samym polu (zabronione w <code>OFK</code>; w <code>4P</code> tworzenie stosu z innym kolorem &mdash; w tym z twoim partnerem po przekątnej &mdash; nigdy nie jest dozwolone). Dowolna grupa twoich własnych Pod, które <b>rozpoczynają turę na tym samym polu</b>, może poruszyć się razem albo rozdzielić wzdłuż osobnych ścieżek kroków i skoków w jednej turze, zapisanych po przecinku (np. <code>44a-45,44beg-34</code>). Ani wystawienie Pod, ani wstawienie Prong nigdy nie łączy się z ruchem.",
            "<b>Zbicie całego stosu i zdjęcie na końcu tury:</b> Przeskoczenie zajętego pola z oznaczeniem <code>x</code> zbija <b>każdy Pod w stosie na tym polu</b>. W zwykłych ruchach pojedynczych i złożonych stosy przeskoczone z oznaczeniem <code>x</code> pozostają fizycznie na planszy jako punkty oparcia do skoków dla pozostałych Pod z tego samego stosu startowego w bieżącej turze i są zdejmowane wspólnie dopiero na końcu tury. Pojedynczy Pod <b>nigdy nie może przeskoczyć dokładnie tego samego pola dwukrotnie</b> w ramach jednego łańcucha skoków.",
            "<b>Dodatek A.1 &mdash; Opróżnienie pola startowego, pętle powrotne i zakaz przeskakiwania Pod z własnego stosu:</b> Przez cały czas trwania swojego łańcucha skoków skaczący Pod <b>nie może przeskoczyć swojego pola startowego <code>S</code> ani żadnego Pod, z którym rozpoczął turę w jednym stosie</b> (dlatego na planszy Bez krawędzi <code>_ED</code> zawijany skok <code>44bcd-66-86-16-34-54</code> nad nieruchomym <code>A44a</code> na <code>44</code> jest nielegalny). Ponadto skaczący Pod może <b>wylądować z powrotem na <code>S</code></b> na końcu pętli skoków <b>wyłącznie wtedy, gdy wszystkie Pod zaczynające turę na <code>S</code> opuszczą pole <code>S</code></b> w trakcie tego ruchu (na przykład złożone opróżnienie pola <code>44a-45,44beg-66x-64x-44</code> jest legalne, ponieważ <code>44</code> zostaje całkowicie opróżnione, natomiast pojedynczy ruch <code>44beg-66x-64x-44</code> zostawiający nieruchomy <code>A44a</code> na <code>44</code> jest nielegalny). Zamknięte pętle skoków bez zbicia (<code>S &rarr; ... &rarr; S</code> przy pełnym opróżnieniu <code>S</code>) są legalne i liczą się jako ruchy bez postępu (<code>+1</code> do progu remisu).",
            "<b>Zakres i warunki wyzwolenia Pod (<code>2P_3B*</code> i <code>4P*</code>):</b> Wyzwolenie zbitego Pod (<code>xy*</code>) obowiązuje wyłącznie w partiach <b>o 3 bazy (<code>2P_3B*</code>) oraz dla 4 graczy (<code>4P*</code>)</b> (nigdy w trybie nagłej śmierci o 1 bazę ani w <code>OFK</code>), zawsze wyzwala 1 z <b>twoich własnych</b> zbitych Pod (<code>capturedPods &gt; 0</code>) i zawsze wymaga, aby docelowe pole OCTI było <b>już zajęte przez co najmniej jeden twój własny Pod</b> (nigdy puste ani zajęte wyłącznie przez partnera). W <code>2P_3B*</code> wyzwolenie na własną bazę domową wymaga dodatkowo zajmowania co najmniej jednej bazy przeciwnika; wyzwolenie bezpośrednio na już zajmowaną bazę przeciwnika wymaga jedynie <code>capturedPods &gt; 0</code>. W <code>4P*</code> wyzwolenie na własną lub partnerską bazę narożną wymaga osobistego zajmowania neutralnego (<code>53</code>/<code>57</code>) lub wrogiego pola OCTI (nawet jeśli partner został wyeliminowany, <code>xy*</code> na jego bazie narożnej wyzwala twój własny Pod i nigdy nie przywraca wyeliminowanego partnera do gry).",
            "<b>Toroidalne zawijanie planszy Bez krawędzi (<code>_ED</code>):</b> Kolumna <code>1</code> łączy się z kolumną <code>9</code>, a rząd <code>1</code> z rzędem <code>9</code> (włącznie z przekątnymi) zarówno dla kroków o 1 pole (<code>15g-95</code>), jak i skoków o 2 pola (<code>97b-29x</code> ze skokiem nad <code>B18e</code>).",
            "<b>SuperProng i bonusowe ruchy wstępne (<code>_SP</code>, <code>;</code>):</b> Złoty SuperProng (<code>!</code>) po wstawieniu jest trwały. Przed swoją regularną akcją tury możesz wykonać <b>po 1 bonusowym ruchu wstępnym na każdy kontrolowany przez ciebie SuperProng na planszy</b> (do <b>2</b> w <code>2P_SP</code> oraz do <b>4</b> w <code>4P_SP</code> po przejęciu wrogich SuperProng), rozdzielając je średnikami (np. <code>44ab!-66x;66ab!-67</code>). Każdy ruch wstępny przemieszcza 1 Pod ściśle w <b>linii prostej</b> (krok o 1 pole lub współliniowy łańcuch skoków bez zmiany kierunku) wzdłuż odrębnego gniazda SuperProng, które nie wykonało jeszcze bonusowego ruchu wstępnego w tej turze. Stosy przeskoczone z oznaczeniem <code>x</code> podczas bonusowego ruchu wstępnego SuperProng są <b>zdejmowane natychmiast</b> po tym ruchu wstępnym, zanim rozpocznie się kolejny ruch wstępny SuperProng lub akcja regularna! Pod, który poruszył się w bonusowym ruchu wstępnym, nadal może użyć <b>dowolnego</b> ze swoich Prong w regularnej fazie tury, a <b>warunek zwycięstwa jest sprawdzany dopiero po zakończeniu pełnej tury</b>.",
            "Rysunek 6: Stos [A44a, A44beg] rozdzielający się ruchem 44a-45,44beg-34 lub całkowicie opróżniający pole 44 ruchem 44a-45,44beg-66x-64x-44, dzięki czemu A44beg legalnie ląduje na opróżnionym polu 44 (Dodatek A.1).",
            "Rysunek 8: Plansza Bez krawędzi (_ED): zawijany krok 15g-95 oraz ukośny skok ze zbiciem 97b-29x przez łączoną krawędź.",
            "Rysunek 9: Prostoliniowy bonusowy ruch wstępny złotego SuperProng 44ab!-66x; (1 ruch wstępny na każdy kontrolowany SuperProng na planszy) natychmiast zdejmujący zbity B55e! przed regularnym krokiem 66ab!-67, ze sprawdzeniem zwycięstwa po zakończeniu pełnej tury.",
            "Rozdział 4 &mdash; Zwycięstwo, eliminacja w 4P, blokada i remis bez postępu",
            "<b>Rodzina <code>2P_3B</code> (standard, 3 bazy):</b> Wygrywasz po zakończeniu swojej pełnej tury, jeśli twoje Pod zajmują jednocześnie <b>wszystkie 3</b> bazy domowe przeciwnika (<code>37</code>, <code>57</code>, <code>77</code> dla Gracza A; <code>33</code>, <code>53</code>, <code>73</code> dla Gracza B).",
            "<b>Rodzina <code>2P_1B</code> (blitz o 1 bazę):</b> Wygrywasz po zakończeniu swojej pełnej tury, gdy twój Pod zajmuje <b>którąkolwiek z 3</b> baz domowych przeciwnika.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Wygrywasz po zakończeniu swojej pełnej tury, gdy twój Pod zajmuje <b>którąkolwiek z 4</b> baz domowych przeciwnika (<code>26, 36, 46, 56</code> dla Gracza A; <code>22, 32, 42, 52</code> dla Gracza B).",
            "<b>Rodzina drużynowa <code>4P</code> (Drużyna A i C kontra Drużyna B i D):</b> Partnerzy siedzący po przekątnej wygrywają wspólnie po zakończeniu pełnej tury, gdy sojusznicze Pod zajmują jednocześnie <b>obie narożne bazy przeciwników</b> (<code>37</code> i <code>73</code> dla Drużyny A i C; <code>33</code> i <code>77</code> dla Drużyny B i D).",
            "<b>Trwała eliminacja przez zajęcie bazy w 4P i bezpieczna baza partnera:</b> W wariancie <code>4P</code>, jeśli <b>wrogi</b> Pod zakończy pełną turę na twojej narożnej bazie domowej (lub wszystkie twoje Pod zostaną zbite i nie pozostanie ci żaden rezerwowy Pod), zostajesz <b>trwale wyeliminowany</b> do końca partii: wszystkie twoje pozostałe na planszy Pod trafiają do twojej puli zbitych Pod (<code>capturedPods</code>), a <b>wszystkie kołki Prong (zwykłe i SuperProng) ze zdjętych Pod przechodzą do wrogiego gracza, który zajął twoją bazę</b>. Wyeliminowane miejsca są pomijane w kolejności tur, podczas gdy ocalały partner gra dalej. Z kolei <b>wejście krokiem lub skokiem na pustą (lub zajętą już przez twoje własne Pod) bazę startową partnera po przekątnej jest legalne i nie pociąga za sobą żadnych konsekwencji &mdash; nigdy nie eliminuje twojego partnera z gry</b> (jeśli partner tymczasowo zajmuje twoją bazę startową, gdy na planszy nie masz żadnego Pod, po prostu tracisz turę, dopóki jej nie opuści).",
            "<b>Całkowite zbicie / Blokada:</b> Ponieważ przekładanie Prong jest zabronione, gracz bez rezerwowych Pod i bez rezerwowych Prong, którego wszystkie Pod na planszy są całkowicie otoczone bez możliwości ruchu, ma <b>zero legalnych ruchów</b>. Jeśli następny gracz w <code>2P</code> / <code>OFK</code> (lub oba miejsca przeciwników w <code>4P</code>) nie ma żadnego legalnego ruchu, gracz lub drużyna, która właśnie wykonała ruch, natychmiast wygrywa (w <code>4P</code> gracz objęty blokadą traci turę, jeśli jego partner wciąż ma legalny ruch). Partie z zegarem kończą się również z chwilą przekroczenia czasu lub poddania.",
            "<b>Uniwersalny remis bez postępu (<code>NO_PROGRESS_DRAW</code>, <code>noProgressPlies</code>):</b> Partia kończy się automatycznie <b>remisem (<code>NO_PROGRESS_DRAW</code>)</b>, gdy <b><code>30</code> kolejnych ruchów (po <code>15</code> pełnych tur na gracza)</b> w dowolnym wariancie <code>2P*</code> lub <code>OFK</code> &mdash; albo <b><code>60</code> kolejnych ruchów (<code>15</code> pełnych rund)</b> w dowolnym wariancie <code>4P*</code> &mdash; upłynie bez żadnego <b>zdarzenia postępu</b>: zbicia Pod (<code>x</code>, w tym podczas bonusowych ruchów wstępnych <code>_SP</code>), wystawienia rezerwowego Pod (<code>xy</code>) lub wyzwolenia (<code>xy*</code>), wstawienia Prong/SuperProng (<code>+d</code>/<code>+d!</code>) bądź trwałej eliminacji gracza w <code>4P</code>. Warunek ten jest sprawdzany na końcu tury <b>po</b> weryfikacji zwycięstwa przez zajęcie baz, całkowitego zbicia oraz blokady (dlatego wygrana osiągnięta w <code>30.</code>/<code>60.</code> ruchu pozostaje ważna).",
            "Rysunek 10: Tryb drużynowy dla 4 graczy (4P): sojusznicy po przekątnej A(33) i C(77) kontra B(37) i D(73), stos jednego koloru [A55ab, A55f], Pod partnera C65cf, bezpieczne wejście na bazę partnera oraz neutralne pola 53 i 57.",
            "Rysunek 11: OCTI for Kids (OFK) na siatce 6×7: po 4 bazy na stronę, 0 rezerwowych Pod, trwałe Prong, brak stosów i zwycięstwo za 1 bazę (34a-36x).",
            "Rysunek 12: Pozycja zwycięstwa w 2P_3B (status = O), w której Gracz A zajmuje jednocześnie wszystkie 3 bazy przeciwnika (A37c, A57a, A77g).",
            "Rozdział 5 &mdash; Macierz referencyjna 13 kanonicznych wariantów",
            "Każda partia w wersji Web, dla Windows, Android, w Archiwum oraz w Bot SDK korzysta z jednego z <b>13 kanonicznych kodów wariantów</b>:",
            "Kod", "Siatka", "Gracze", "Pod (Start+Rez.)", "Prong (Zw./SP)", "Modyfikatory i cel zwycięstwa", "Ranking",
            "Tak", "Towarzyska",
            "Zasady standardowe; zajmij wszystkie 3 bazy przeciwnika",
            "SuperProng <code>!</code> + <code>;</code> proste ruchy wstępne (&le;2); 3 bazy",
            "Toroidalna plansza Bez krawędzi; 3 bazy",
            "SuperProng (&le;2 ruchy wstępne) + Bez krawędzi; 3 bazy",
            "Blitz z nagłą śmiercią; zajmij dowolną bazę przeciwnika",
            "SuperProng (&le;2 ruchy wstępne) + nagła śmierć o 1 bazę",
            "Bez krawędzi + nagła śmierć o 1 bazę",
            "SuperProng (&le;2 ruchy wstępne) + Bez krawędzi + 1 baza",
            "Neutralne <code>53/57</code>, trwała eliminacja po zajęciu bazy; 2 bazy",
            "Drużynowe 4P + proste ruchy wstępne SuperProng (&le;4)",
            "Drużynowe 4P + plansza Bez krawędzi",
            "Drużynowe 4P + SuperProng (&le;4 ruchy wstępne) + Bez krawędzi",
            "Po 4 bazy na stronę, bez rezerwy, bez stosów i ruchów złożonych; 1 baza",
            "Rozdział 6 &mdash; Oficjalna notacja ruchów i specyfikacja OCTI-FEN",
            "Gramatyka notacji ruchów",
            "Składnia", "Akcja", "Przykład",
            "Wystawienie rezerwowego Pod na pole bazowe OCTI",
            "Wyzwolenie własnego zbitego Pod na zajęte pole OCTI (tylko <code>2P_3B*</code> i <code>4P*</code>)",
            "Wstawienie zwykłego Prong do gniazda <code>d &isin; {a..h}</code>",
            "Wstawienie złotego SuperProng do gniazda <code>d</code>",
            "Krok o 1 pole bez zbicia",
            "Pojedynczy skok lub skok wielokrotny (<code>x</code> = opcjonalne zbicie całego stosu)",
            "Ruch złożony rozdzielający Pod z tego samego pola startowego",
            "1&ndash;4 proste ruchy wstępne SuperProng (po 1 na SP na planszy; natychmiastowe zdjęcie <code>x</code>) + ruch regularny",
            "Format zapisu pozycji OCTI-FEN",
            "Stany planszy są serializowane do rozdzielanych dwukropkami ciągów <b>OCTI-FEN</b> (11 elementów dla <code>2</code> / <code>OFK</code>; 13 elementów dla <code>4</code>):",
            "<code>mode</code>: <code>2</code>, <code>4</code> lub <code>OFK</code> &bull; <code>bases</code>: <code>3</code> lub <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> lub <code>1</code>.",
            "<code>status</code>: <code>N</code> (nierozpoczęta), <code>P</code> (w toku), <code>O</code> (zakończona), <code>R</code> (poddana).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> lub <code>-</code> &bull; <code>elimMask</code>: maska bitowa eliminacji w 4P (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (np. <code>4,0,25,0</code>).",
            "<code>squares</code>: rozdzielone przecinkami Pod w kolejności wierszowej <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (np. <code>A44ab!,B55e!</code>) lub <code>-</code>, gdy plansza jest pusta.",
            "<code>noProgressPlies</code> (opcjonalny 12./14. element końcowy): kolejne ruchy bez postępu (<code>0..1000</code>, domyślnie <code>0</code> w razie pominięcia; kanoniczne <code>toPositionString()</code> zwraca ciąg 11/13-elementowy).",
            "Rozdział 7 &mdash; Dwupulowy system Elo i zasady Areny turniejowej",
            "Oficjalne rankingi startują od <b>1200 Elo</b> w trzech niezależnych drabinkach wariantów 1 na 1: <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b> oraz <b><code>OFK</code></b>. Wszystkie partie dla 4 graczy (<code>4P*</code>), z SuperProng (<code>*_SP</code>), Bez krawędzi (<code>*_ED</code>), warianty łączone (<code>*_SPED</code>) oraz partie offline są towarzyskie (nierankingowe).",
            "<b>Pula 1 &mdash; Drabinki tylko dla ludzi (Ranking i lista graczy):</b><br>Uwzględnia wyłącznie sieciowe partie rankingowe Człowiek kontra Człowiek przy standardowym współczynniku Elo <code>K = 32</code>. Rankingi ludzi są całkowicie odizolowane od partii z robotami.",
            "<b>Pula 2 &mdash; Otwarta Arena Robotów i Ludzi (Ranking i katalog robotów):</b><br>Uwzględnia sieciowe partie rankingowe z udziałem co najmniej jednego zarejestrowanego robota SI (Człowiek kontra Robot oraz Robot kontra Robot) z trzema zabezpieczeniami przed sztucznym nabijaniem rankingu:<ul><li><b>Partie Człowiek kontra Robot:</b> Nielimitowana gra rankingowa przy standardowym <code>K = 32</code> (również przeciwko własnym robotom).</li><li><b>Współczynniki K w partiach Robot kontra Robot:</b> Partie między różnymi właścicielami używają standardowego <code>K = 32</code>; testy wewnętrzne robotów tego samego właściciela używają obniżonego <code>K = 16</code>.</li><li><b>Limit pary Robot kontra Robot i ochrona przed wczesnym poddaniem:</b> Partie Robot kontra Robot są ograniczone do <b>4 partii rankingowych w kroczącym oknie 24-godzinnym</b> na dowolną parę robotów (niezależnie od kolejności; 5. i kolejne automatycznie stają się towarzyskie; brak dziennego limitu przeciwko różnym robotom), a poddanie przed <b>8. ruchem</b> zapisuje zmianę 0 Elo.</li></ul>"
        },
        /* 13. Italian (it) */
        {
            "it",
            {
                { "ch1", "Capitolo 1", "Geometria della scacchiera, Pod, Prong e preparazione", "Coordinate 9x9, riserve iniziali e 8 fori direzionali a–h" },
                { "ch2", "Capitolo 2", "Le azioni del turno", "Inserimento di Prong, passo di 1 casella, catena multisalto, riserva e liberazione" },
                { "ch3", "Capitolo 3", "Impilamento, mosse composte e Appendice A", "Mosse composte con virgola, anelli A.1, Senza bordi e SuperProng" },
                { "ch4", "Capitolo 4", "Vittoria ed eliminazione 4P", "Occupazione di 3 o 1 base, eliminazione 4P e patta senza progresso dopo 30/60 mosse" },
                { "ch5", "Capitolo 5", "Matrice delle 13 varianti canoniche", "Dimensioni, dotazione di Pod e Prong, modificatori e validità Elo" },
                { "ch6", "Capitolo 6", "Notazione delle mosse e grammatica OCTI-FEN", "Grammatica algebrica ufficiale e formato OCTI-FEN a 11/13 campi" },
                { "ch7", "Capitolo 7", "Regole Elo a doppio bacino e Arena", "Bacino 1 solo umani, Bacino 2 Arena aperta Robot e Umani e relative tutele" }
            },
            "Regolamento ufficiale illustrato di OCTI",
            "Regole complete multivariante, meccaniche tattiche, casi limite dell’Appendice A, grammatica ufficiale di mosse e OCTI-FEN e regolamento competitivo Elo a doppio bacino.",
            "Capitoli:",
            "Capitolo 1 &mdash; Geometria della scacchiera, Pod, Prong e preparazione iniziale",
            "OCTI è un gioco di strategia astratta deterministico e a informazione perfetta basato sulla costruzione di vettori di movimento. Ogni Pod entra sulla scacchiera vuoto (con zero direzioni di movimento) e acquisisce mobilità direzionale turno dopo turno man mano che i giocatori inseriscono ciascun <b>Prong</b>.",
            "Pod (pedina ottagonale principale con 8 fori direzionali)",
            "Prong (piolo direzionale inserito in un Pod per sbloccare il movimento)",
            "SuperProng (Prong dorato che consente uno spostamento preludio bonus in linea retta)",
            "<b>Griglia standard 9&times;9:</b> Le colonne <code>x = 1..9</code> procedono da ovest a est (da sinistra a destra); le traverse <code>y = 1..9</code> procedono da sud a nord (dal basso verso l’alto). Ogni casella si scrive con due cifre <code>xy</code> (ad esempio, <code>53</code> indica colonna 5, traversa 3).",
            "<b>Risorse iniziali per 2 giocatori (<code>2P_3B</code> e <code>2P_1B</code>):</b> Il Giocatore A inizia con <b>3 Pod</b> sulle proprie basi iniziali OCTI <code>33</code>, <code>53</code> e <code>73</code>; il Giocatore B inizia con <b>3 Pod</b> sulle proprie basi iniziali OCTI <code>37</code>, <code>57</code> e <code>77</code>. Ciascun giocatore dispone inoltre di <b>4 Pod di riserva</b> (7 Pod in totale per parte) e di <b>25 Prong</b> (oppure <b>24 Prong normali + 1 SuperProng dorato</b> nelle varianti <code>_SP</code>).",
            "<b>8 fori direzionali per Prong (<code>a</code>&ndash;<code>h</code>):</b> Ogni Pod ottagonale presenta 8 fori perimetrali disposti in senso orario a partire da nord: <code>a</code>=N (<code>0,+1</code>), <code>b</code>=NE (<code>+1,+1</code>), <code>c</code>=E (<code>+1,0</code>), <code>d</code>=SE (<code>+1,-1</code>), <code>e</code>=S (<code>0,-1</code>), <code>f</code>=SO (<code>-1,-1</code>), <code>g</code>=O (<code>-1,0</code>) e <code>h</code>=NO (<code>-1,+1</code>).",
            "Figura 1: Preparazione standard 9x9 per 2 giocatori (2P_3B) con le traverse iniziali 33/53/73 contro 37/57/77.",
            "Figura 2: 2P_1B (Blitz a 1 base) usa la stessa preparazione, ma occupare una qualsiasi delle basi iniziali nemiche (55ab-57x) è sufficiente per vincere.",
            "Figura 3: Gli 8 fori direzionali a–h attorno al Pod A55acg e l’inserimento di un Prong dalla scorta (55acg+b) in un foro perimetrale vuoto.",
            "Capitolo 2 &mdash; Le azioni del turno",
            "Il Giocatore A muove per primo e i turni si alternano rigorosamente (in <code>4P</code> l’ordine di turno ruota <code>A &rarr; B &rarr; C &rarr; D</code>, escludendo i posti eliminati). Passare il turno è vietato; nel tuo turno devi eseguire <b>esattamente una</b> azione regolare del turno:",
            "<b>Inserire un Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Prendi 1 Prong dalla tua scorta disponibile e inseriscilo in un foro vuoto di uno dei tuoi Pod sulla scacchiera (ad es. <code>55acg+b</code>; oppure <code>33+b!</code> per un SuperProng dorato). Una volta inseriti, i Prong sono permanenti su quel Pod &mdash; spostare un Prong installato in un altro foro non è mai consentito in alcuna variante.",
            "<b>Passo di 1 casella (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Muovi un Pod di 1 casella lungo una delle direzioni dei suoi Prong installati verso una casella vuota o una pila di tuoi Pod (ad es. <code>44ab-45</code>). Un passo di 1 casella <b>non cattura mai</b> e non può entrare in una casella occupata dal Pod di un altro giocatore (incluso il tuo compagno diagonale in <code>4P</code>).",
            "<b>Salto di 2 caselle e catena multisalto (<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> Salta di 2 caselle lungo la direzione di un Prong installato scavalcando una casella adiacente occupata (propria/alleata o nemica) per atterrare sulla casella vuota immediatamente oltre. Puoi concatenare ulteriori salti con quello stesso Pod lungo una qualsiasi delle direzioni dei suoi Prong installati (cambiando direzione durante la catena) e fermarti anticipatamente dopo qualsiasi salto legale.",
            "<b>Schierare un Pod di riserva (<code>xy</code>) o liberare un Pod catturato (<code>xy*</code>):</b> Piazza 1 Pod di riserva sulla tua base iniziale quando è vuota o occupata solo dai tuoi Pod (oppure su una base nemica che occupi già), o &mdash; nelle partite a 3 basi (<code>2P_3B*</code>) e a 4 giocatori (<code>4P*</code>) &mdash; libera 1 dei tuoi Pod catturati (<code>xy*</code>) su una casella OCTI che occupi già quando ne soddisfi i requisiti (la liberazione non si applica alle varianti a morte improvvisa a 1 base né a <code>OFK</code>).",
            "<b>Cattura opzionale dell’intera pila e bottino di Prong:</b> Quando salti una casella occupata, la cattura è <b>opzionale</b>: aggiungi <code>x</code> (ad es. <code>44ab-66x</code>) per catturare <b>tutti i Pod presenti nella pila saltata</b> (cattura dell’intera pila &mdash; la cattura parziale o selettiva non è mai consentita), oppure ometti <code>x</code> (ad es. <code>44ab-66</code>) per lasciare intatta la pila saltata. Durante le mosse regolari singole o composte, le pile catturate vengono rimosse a fine turno e trasferite tra i Pod prigionieri (<code>capturedPods</code>) dei rispettivi proprietari, mentre <b>tutti i Prong inseriti in esse passano nella scorta disponibile di Prong del giocatore che ha effettuato la cattura</b>.",
            "Figura 4: Il Pod A44ab avanza di un passo di 1 casella a nord (44ab-45) rispetto al salto di 2 caselle a NE oltre B55e con cattura opzionale dell’intera pila (44ab-66x).",
            "Figura 5: Catena multisalto 33abd-55-57x-75x che scavalca il proprio Pod A44h, vira a nord per catturare B56e e poi a SE per catturare B66c.",
            "Figura 7: Schieramento di un Pod disponibile di riserva sulla base iniziale vuota 33 rispetto alla liberazione di un proprio Pod catturato (53* o 57*) in 2P_3B* e 4P* una volta occupata una casella OCTI nemica (o neutrale in 4P).",
            "Capitolo 3 &mdash; Impilamento, mosse composte e casi limite dell’Appendice A",
            "<b>Impilamento di Pod propri e mosse composte multi-Pod (<code>,</code>):</b> Più Pod del tuo stesso colore possono impilarsi sulla stessa casella (vietato in <code>OFK</code>; in <code>4P</code> impilarsi con un altro colore &mdash; incluso il tuo compagno diagonale &mdash; non è mai consentito). Uno o più dei tuoi Pod che <b>iniziano il turno sulla stessa casella</b> possono muoversi insieme o dividersi lungo percorsi separati di passo o salto in un unico turno, uniti da virgole (ad es. <code>44a-45,44beg-34</code>). Il posizionamento di Pod e l’inserimento di Prong non si combinano mai con il movimento.",
            "<b>Cattura dell’intera pila e rimozione a fine turno:</b> Saltare una casella occupata con <code>x</code> cattura <b>ogni Pod nella pila di quella casella</b>. Nelle mosse regolari singole e composte, le pile saltate con <code>x</code> restano sulla scacchiera come perni di salto utilizzabili dagli altri compagni di pila durante lo stesso turno e vengono rimosse insieme a fine turno. Uno stesso Pod non può <b>mai saltare due volte esattamente la stessa casella</b> nel corso di una singola catena di salti.",
            "<b>Appendice A.1 &mdash; Svuotamento della casella di partenza, salti ad anello e divieto di saltare compagni di pila:</b> Per l’intera durata della sua catena di salti, un Pod in salto <b>non può saltare la propria casella di partenza <code>S</code> né alcun Pod con cui ha iniziato il turno impilato</b> (pertanto un salto avvolgente Senza bordi <code>_ED</code> come <code>44bcd-66-86-16-34-54</code> sopra <code>A44a</code> rimasto fermo su <code>44</code> è illegale). Inoltre, un Pod in salto può <b>riatterrare su <code>S</code></b> al termine di un anello multisalto <b>solo quando tutti i Pod che hanno iniziato il turno su <code>S</code> svuotano <code>S</code></b> nel corso della mossa (ad esempio, lo svuotamento composto <code>44a-45,44beg-66x-64x-44</code> è legale perché <code>44</code> viene completamente svuotata, mentre la mossa singola <code>44beg-66x-64x-44</code> che lascia <code>A44a</code> fermo su <code>44</code> è illegale). I salti circolari multisalto senza cattura (<code>S &rarr; ... &rarr; S</code> quando <code>S</code> è completamente svuotata) restano legali e contano come mosse di solo movimento (<code>+1</code> verso la soglia di patta senza progresso).",
            "<b>Ambito e requisiti della liberazione di Pod (<code>2P_3B*</code> e <code>4P*</code>):</b> Liberare un Pod catturato (<code>xy*</code>) si applica esclusivamente alle partite a <b>3 basi (<code>2P_3B*</code>) e a 4 giocatori (<code>4P*</code>)</b> (mai nella morte improvvisa a 1 base né in <code>OFK</code>), libera sempre 1 dei <b>tuoi</b> Pod catturati (<code>capturedPods &gt; 0</code>) e richiede sempre che la casella OCTI di destinazione sia <b>già occupata da almeno uno dei tuoi Pod</b> (mai una casella vuota o occupata solo dal compagno). In <code>2P_3B*</code>, liberare su una propria base iniziale richiede inoltre di occupare almeno una base nemica; liberare direttamente su una base nemica che occupi già richiede soltanto <code>capturedPods &gt; 0</code>. In <code>4P*</code>, liberare sulla propria base d’angolo o su quella del compagno richiede di occupare personalmente una casella OCTI neutrale (<code>53</code>/<code>57</code>) o nemica (anche se il compagno è eliminato, <code>xy*</code> sulla sua base d’angolo libera un tuo Pod e non riammette mai in gioco il compagno eliminato).",
            "<b>Raccordo toroidale Senza bordi (<code>_ED</code>):</b> La colonna <code>1</code> si raccorda alla colonna <code>9</code> e la traversa <code>1</code> si raccorda alla traversa <code>9</code> (diagonali incluse), sia per i passi (<code>15g-95</code>) sia per i salti di 2 caselle (<code>97b-29x</code> scavalcando <code>B18e</code>).",
            "<b>SuperProng e preludi bonus (<code>_SP</code>, <code>;</code>):</b> Un SuperProng dorato (<code>!</code>) è permanente una volta inserito. Prima della tua azione regolare del turno, puoi eseguire <b>1 spostamento preludio bonus per ciascun SuperProng che controlli sulla scacchiera</b> (fino a <b>2</b> in <code>2P_SP</code> e fino a <b>4</b> in <code>4P_SP</code> quando si catturano SuperProng nemici), separati da punto e virgola (ad es. <code>44ab!-66x;66ab!-67</code>). Ogni preludio sposta 1 Pod rigorosamente in <b>linea retta</b> (passo di 1 casella o catena di salti collineari senza cambi di direzione) lungo un foro SuperProng diverso che non ha ancora attivato un preludio bonus in questo turno. Le pile saltate con <code>x</code> durante un preludio bonus SuperProng vengono <b>rimosse immediatamente</b> al termine di quel preludio, prima che inizi il successivo preludio SuperProng o l’azione regolare! Un Pod che si è spostato in un preludio bonus può comunque usare <b>qualsiasi</b> suo Prong nella fase regolare del turno, e <b>la vittoria viene verificata solo al completamento dell’intero turno</b>.",
            "Figura 6: La pila [A44a, A44beg] si divide tramite 44a-45,44beg-34 oppure svuota completamente 44 tramite 44a-45,44beg-66x-64x-44 così che A44beg riatterri legalmente sulla casella svuotata 44 (Appendice A.1).",
            "Figura 8: Scacchiera Senza bordi (_ED) con passo oltre il bordo 15g-95 e salto diagonale con cattura 97b-29x attraverso il confine toroidale.",
            "Figura 9: Preludio bonus rettilineo con SuperProng dorato 44ab!-66x; (1 spostamento bonus per SuperProng controllato sulla scacchiera) che rimuove immediatamente il Pod catturato B55e! prima del passo regolare 66ab!-67, con verifica della vittoria al completamento dell’intero turno.",
            "Capitolo 4 &mdash; Condizioni di vittoria, eliminazione, blocco e patta senza progresso",
            "<b>Famiglia <code>2P_3B</code> (Standard a 3 basi):</b> Vinci al completamento del tuo intero turno se i tuoi Pod occupano simultaneamente <b>tutte e 3</b> le basi iniziali nemiche (<code>37</code>, <code>57</code>, <code>77</code> per A; <code>33</code>, <code>53</code>, <code>73</code> per B).",
            "<b>Famiglia <code>2P_1B</code> (Blitz a 1 base):</b> Vinci al completamento del tuo intero turno quando un tuo Pod occupa <b>una qualsiasi</b> delle 3 basi iniziali nemiche.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Vinci al completamento del tuo intero turno quando un tuo Pod occupa <b>una qualsiasi</b> delle 4 basi iniziali nemiche (<code>26, 36, 46, 56</code> per A; <code>22, 32, 42, 52</code> per B).",
            "<b>Famiglia a coppie <code>4P</code> (Squadra A e C contro Squadra B e D):</b> I compagni diagonali vincono insieme al completamento di un intero turno quando i Pod alleati occupano simultaneamente <b>entrambe le basi d’angolo nemiche</b> (<code>37</code> e <code>73</code> per la Squadra A e C; <code>33</code> e <code>77</code> per la Squadra B e D).",
            "<b>Eliminazione permanente per occupazione della base in 4 giocatori e base sicura del compagno:</b> In <code>4P</code>, se un Pod <b>nemico</b> conclude il suo intero turno sulla tua base iniziale d’angolo (oppure se tutti i tuoi Pod vengono catturati senza alcun Pod di riserva rimasto), sei <b>eliminato permanentemente</b> per il resto della partita: tutti i tuoi Pod rimasti sulla scacchiera vengono trasferiti tra i tuoi Pod prigionieri (<code>capturedPods</code>), e <b>tutti i Prong (normali e SuperProng) presenti sui tuoi Pod rimossi passano al giocatore nemico che ha occupato la tua base</b>. I posti eliminati vengono esclusi dalla rotazione dei turni mentre il compagno superstite continua a giocare. Al contrario, <b>entrare con un passo o un salto sulla base iniziale vuota del tuo compagno diagonale (o già occupata dai tuoi Pod) è legale e non ha alcuna conseguenza &mdash; non elimina mai il tuo compagno dalla partita</b> (se il tuo compagno occupa temporaneamente la tua base iniziale mentre hai zero Pod sulla scacchiera, perdi semplicemente il turno finché non la svuota).",
            "<b>Cattura totale / Blocco:</b> Poiché è vietato riposizionare i Prong già inseriti, un giocatore con zero Pod di riserva e zero Prong di riserva i cui Pod sulla scacchiera siano completamente chiusi ha <b>zero mosse legali</b>. Se il giocatore successivo in <code>2P</code> / <code>OFK</code> (o entrambi i posti avversari in <code>4P</code>) ha zero mosse legali, il giocatore o la squadra che ha appena mosso vince immediatamente (in <code>4P</code>, un giocatore bloccato perde il turno se il suo compagno ha ancora una mossa legale). Le partite con orologio terminano inoltre per tempo scaduto o abbandono.",
            "<b>Patta senza progresso in tutte le varianti (<code>NO_PROGRESS_DRAW</code>, <code>noProgressPlies</code>):</b> Una partita termina automaticamente in <b>patta (<code>NO_PROGRESS_DRAW</code>)</b> quando vengono completate <b><code>30</code> mosse consecutive (<code>15</code> turni completi per giocatore)</b> in qualsiasi variante <code>2P*</code> o <code>OFK</code> &mdash; oppure <b><code>60</code> mosse consecutive (<code>15</code> giri completi)</b> in qualsiasi variante <code>4P*</code> &mdash; senza alcun <b>evento di progresso</b>: cattura di Pod (<code>x</code>, inclusi i preludi bonus <code>_SP</code>), schieramento di Pod di riserva (<code>xy</code>) o liberazione (<code>xy*</code>), inserimento di Prong o SuperProng (<code>+d</code>/<code>+d!</code>), oppure eliminazione permanente di un giocatore in <code>4P</code>. La condizione viene valutata a fine turno <b>dopo</b> i controlli di vittoria per occupazione delle basi, cattura totale e blocco per assenza di mosse legali (pertanto vincere alla mossa <code>30</code>/<code>60</code> assegna comunque la vittoria).",
            "Figura 10: Partita a coppie per 4 giocatori (4P): alleati diagonali A(33) e C(77) contro B(37) e D(73), pila dello stesso colore [A55ab, A55f], Pod del compagno C65cf, ingresso sicuro nella base del compagno e caselle neutrali 53/57.",
            "Figura 11: OCTI for Kids (OFK) sulla griglia 6x7: 4 basi per parte, 0 Pod di riserva, Prong permanenti, nessun impilamento, vittoria a 1 base (34a-36x).",
            "Figura 12: Posizione di vittoria in 2P_3B (status = O) con il Giocatore A che occupa simultaneamente tutte e 3 le basi nemiche (A37c, A57a, A77g).",
            "Capitolo 5 &mdash; Matrice di riferimento delle 13 varianti canoniche",
            "Ogni partita su Web, Desktop, Android, Archivio e SDK dei robot usa uno dei <b>13 codici di variante canonici</b>:",
            "Codice", "Griglia", "Giocatori", "Pod (Inizio+Ris.)", "Prong (N/SP)", "Modificatori e obiettivo", "Classificata",
            "Sì", "Amichevole",
            "Regole standard; occupare tutte e 3 le basi nemiche",
            "SuperProng <code>!</code> + <code>;</code> preludi bonus rettilinei (&le;2); 3 basi",
            "Raccordo toroidale Senza bordi; 3 basi",
            "SuperProng (&le;2 preludi) + Senza bordi; 3 basi",
            "Blitz a morte improvvisa; occupare una base nemica qualsiasi",
            "SuperProng (&le;2 preludi) + morte improvvisa a 1 base",
            "Senza bordi + morte improvvisa a 1 base",
            "SuperProng (&le;2 preludi) + Senza bordi + 1 base",
            "Neutrali <code>53/57</code>, eliminazione permanente per base; 2 basi nemiche",
            "Coppie 4P + preludi bonus rettilinei SuperProng (&le;4)",
            "Coppie 4P + raccordo Senza bordi",
            "Coppie 4P + SuperProng (&le;4 preludi) + Senza bordi",
            "4 basi per parte, 0 riserve, niente pile né mosse composte; 1 base",
            "Capitolo 6 &mdash; Notazione ufficiale delle mosse e specifica OCTI-FEN",
            "Grammatica della notazione delle mosse",
            "Sintassi", "Azione", "Esempio",
            "Schierare un Pod disponibile di riserva su una casella OCTI",
            "Liberare un proprio Pod catturato su una casella OCTI occupata (solo <code>2P_3B*</code> e <code>4P*</code>)",
            "Inserire un Prong normale nel foro <code>d &isin; {a..h}</code>",
            "Inserire un SuperProng dorato nel foro <code>d</code>",
            "Passo di 1 casella senza cattura",
            "Salto singolo o catena multisalto (<code>x</code> = cattura opzionale dell’intera pila)",
            "Mossa composta che divide più Pod partiti dalla stessa casella",
            "1&ndash;4 preludi bonus rettilinei SuperProng (1 per SP sulla scacchiera; rimozione immediata con <code>x</code>) + turno regolare",
            "Formato della stringa di posizione OCTI-FEN",
            "Gli stati della scacchiera vengono serializzati in stringhe <b>OCTI-FEN</b> separate da due punti (11 campi per <code>2</code> / <code>OFK</code>; 13 campi per <code>4</code>):",
            "<code>mode</code>: <code>2</code>, <code>4</code> oppure <code>OFK</code> &bull; <code>bases</code>: <code>3</code> oppure <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> oppure <code>1</code>.",
            "<code>status</code>: <code>N</code> (non iniziata), <code>P</code> (in corso), <code>O</code> (fine partita), <code>R</code> (abbandono).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> oppure <code>-</code> &bull; <code>elimMask</code>: maschera di bit eliminazione 4P (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (ad es. <code>4,0,25,0</code>).",
            "<code>squares</code>: Pod separati da virgole in ordine per righe <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (ad es. <code>A44ab!,B55e!</code>), oppure <code>-</code> se vuota.",
            "<code>noProgressPlies</code> (12°/14° campo finale opzionale): mosse consecutive senza progresso (<code>0..1000</code>, predefinito <code>0</code> se omesso; la funzione canonica <code>toPositionString()</code> emette la stringa a 11/13 campi).",
            "Capitolo 7 &mdash; Classifica Elo a doppio bacino e regole dell’Arena",
            "I punteggi ufficiali partono da <b>1200 Elo</b> su tre classifiche 1 contro 1 indipendenti: <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b> e <b><code>OFK</code></b>. Tutte le partite a 4 giocatori (<code>4P*</code>), SuperProng (<code>*_SP</code>), Senza bordi (<code>*_ED</code>), combinate (<code>*_SPED</code>) e offline sono amichevoli (non classificate).",
            "<b>Bacino 1 &mdash; Classifiche solo umani (Classifica ed elenco giocatori):</b><br>Registra esclusivamente le partite classificate online Umano contro Umano usando il fattore Elo standard <code>K = 32</code>. I punteggi solo umani sono completamente isolati dalle partite con i robot.",
            "<b>Bacino 2 &mdash; Arena aperta Robot e Umani (Classifica ed elenco dei robot):</b><br>Registra le partite classificate online che coinvolgono almeno un robot IA registrato (Umano contro Robot e Robot contro Robot) con tre tutele contro la manipolazione del punteggio:<ul><li><b>Partite Umano contro Robot:</b> Partite classificate illimitate con fattore standard <code>K = 32</code> (anche contro i propri robot).</li><li><b>Fattori K Robot contro Robot:</b> Le sfide tra proprietari diversi usano il fattore standard <code>K = 32</code>; i test interni dello stesso proprietario usano il fattore ridotto <code>K = 16</code>.</li><li><b>Limite per coppia Robot contro Robot e tutela contro l’abbandono anticipato:</b> Le sfide Robot contro Robot sono limitate a <b>4 partite classificate per finestra mobile di 24 ore</b> per ciascuna coppia di robot (in qualsiasi ordine; dalla 5ª in poi passano automaticamente ad amichevoli; nessun limite giornaliero contro robot avversari distinti), e gli abbandoni prima di <b>8 mosse</b> registrano variazione Elo pari a 0.</li></ul>"
        },
        /* 14. Indonesian (id) */
        {
            "id",
            {
                { "ch1", "Bab 1", "Geometri Papan, Pod, Prong & Susunan Awal", "Koordinat 9x9, cadangan awal, dan 8 soket arah mata angin a-h" },
                { "ch2", "Bab 2", "Aksi dalam Satu Giliran", "Memasang Prong, langkah 1 petak, lompatan beruntun, cadangan & pembebasan" },
                { "ch3", "Bab 3", "Tumpukan, Langkah Komposit & Lampiran A", "Langkah multi-Pod dengan koma, putaran A.1, Tanpa Tepi & SuperProng" },
                { "ch4", "Bab 4", "Kemenangan & Eliminasi 4P", "Pendudukan 3 markas vs 1 markas, eliminasi 4P & seri 30/60 langkah tanpa kemajuan" },
                { "ch5", "Bab 5", "Matriks 13 Varian Resmi", "Ukuran papan, jatah Pod/Prong, aturan khusus & kelayakan peringkat" },
                { "ch6", "Bab 6", "Notasi Langkah & Tata Bahasa OCTI-FEN", "Tata bahasa notasi aljabar resmi dan format OCTI-FEN 11/13 ruas" },
                { "ch7", "Bab 7", "Aturan Elo Dua Kelompok & Arena", "Kelompok 1 Khusus Manusia vs Kelompok 2 Arena Terbuka Robot & Manusia" }
            },
            "Buku Aturan Bergambar Resmi OCTI",
            "Aturan lengkap lintas varian, mekanika taktis, kasus khusus Lampiran A, tata bahasa langkah/OCTI-FEN resmi, serta regulasi kompetitif Elo Dua Kelompok.",
            "Daftar Bab:",
            "Bab 1 &mdash; Geometri Papan, Pod, Prong &amp; Susunan Awal",
            "OCTI adalah permainan strategi abstrak deterministik berinformasi sempurna berbasis perakitan vektor gerak. Setiap Pod masuk ke papan dalam keadaan kosong (tanpa arah gerak) dan memperoleh mobilitas terarah seiring pemain memasang <b>Prong</b> satu demi satu pada gilirannya.",
            "Pod (bidak utama segi delapan dengan 8 soket arah di sekelilingnya)",
            "Prong (pasak arah yang dimasukkan ke soket Pod untuk membuka arah gerak)",
            "SuperProng (Prong emas yang memberikan langkah awal bonus garis lurus)",
            "<b>Kisi Standar 9&times;9:</b> Kolom <code>x = 1..9</code> membentang dari Barat ke Timur (kiri ke kanan); baris <code>y = 1..9</code> membentang dari Selatan ke Utara (bawah ke atas). Setiap petak ditulis sebagai dua digit <code>xy</code> (misalnya, <code>53</code> adalah kolom 5, baris 3).",
            "<b>Sumber Daya Awal 2 Pemain (<code>2P_3B</code> &amp; <code>2P_1B</code>):</b> Pemain A memulai dengan <b>3 Pod</b> di Markas Utama OCTI <code>33</code>, <code>53</code>, dan <code>73</code>; Pemain B memulai dengan <b>3 Pod</b> di Markas Utama OCTI <code>37</code>, <code>57</code>, dan <code>77</code>. Masing-masing pemain juga memegang <b>4 Pod Cadangan</b> (total 7 Pod per sisi) serta <b>25 Prong</b> (atau <b>24 Prong biasa + 1 SuperProng emas</b> pada varian <code>_SP</code>).",
            "<b>8 Soket Arah Mata Angin Prong (<code>a</code>&ndash;<code>h</code>):</b> Setiap Pod segi delapan memiliki 8 soket keliling yang dipetakan searah jarum jam dari Utara: <code>a</code>=U (<code>0,+1</code>), <code>b</code>=TL (<code>+1,+1</code>), <code>c</code>=T (<code>+1,0</code>), <code>d</code>=TG (<code>+1,-1</code>), <code>e</code>=S (<code>0,-1</code>), <code>f</code>=BD (<code>-1,-1</code>), <code>g</code>=B (<code>-1,0</code>), dan <code>h</code>=BL (<code>-1,+1</code>).",
            "Gambar 1: Susunan awal standar 9x9 untuk 2 Pemain (2P_3B) dengan baris markas 33/53/73 melawan 37/57/77.",
            "Gambar 2: 2P_1B (Kilat 1 Markas) memakai susunan awal yang sama persis, tetapi menduduki 1 markas lawan mana pun (55ab-57x) sudah cukup untuk menang.",
            "Gambar 3: Delapan soket arah mata angin a-h di sekeliling Pod A55acg dan pemasangan Prong persediaan (55acg+b) ke soket keliling yang kosong.",
            "Bab 2 &mdash; Aksi dalam Satu Giliran",
            "Pemain A melangkah lebih dulu dan giliran bergantian secara ketat (pada <code>4P</code>, urutan giliran berputar <code>A &rarr; B &rarr; C &rarr; D</code> dengan melewati kursi yang telah tereliminasi). Melewatkan giliran dilarang; pada giliran Anda, Anda wajib menjalankan <b>tepat satu</b> aksi giliran reguler:",
            "<b>Pasang Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Ambil 1 Prong dari persediaan Prong Anda dan masukkan ke soket kosong pada salah satu Pod Anda di papan (mis. <code>55acg+b</code>; atau <code>33+b!</code> untuk SuperProng emas). Begitu dipasang, Prong bersifat permanen pada Pod tersebut&mdash;memindahkan Prong yang sudah terpasang ke soket lain tidak pernah diizinkan dalam varian apa pun.",
            "<b>Langkah 1 Petak (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Gerakkan satu Pod sejauh 1 petak mengikuti salah satu arah Prong yang terpasang ke petak kosong atau tumpukan Pod milik Anda sendiri (mis. <code>44ab-45</code>). Langkah 1 petak <b>tidak pernah menangkap</b> dan tidak dapat memasuki petak yang ditempati Pod pemain lain (termasuk rekan diagonal Anda pada <code>4P</code>).",
            "<b>Lompatan 2 Petak &amp; Rantai Lompatan Beruntun (<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> Melompat sejauh 2 petak mengikuti arah Prong yang terpasang melewati satu petak bersebelahan yang terisi (kawan maupun lawan) ke petak kosong di baliknya. Anda dapat merangkai lompatan tambahan dengan Pod yang sama mengikuti arah Prong terpasang mana pun padanya (berbelok di tengah rantai) dan boleh berhenti lebih awal setelah lompatan sah mana pun.",
            "<b>Tempatkan Pod Cadangan (<code>xy</code>) atau Bebaskan Pod Tertawan (<code>xy*</code>):</b> Letakkan 1 Pod Cadangan di Markas Utama Anda sendiri saat kosong atau hanya ditempati oleh Pod milik Anda sendiri (atau di markas lawan yang sudah Anda duduki), atau&mdash;pada pertandingan 3 Markas (<code>2P_3B*</code>) dan 4 Pemain (<code>4P*</code>)&mdash;bebaskan 1 Pod milik Anda sendiri yang tertawan (<code>xy*</code>) ke petak OCTI yang sudah Anda duduki apabila memenuhi syarat (pembebasan tidak berlaku pada varian penentuan mendadak 1 Markas maupun <code>OFK</code>).",
            "<b>Penangkapan Seluruh Tumpukan Opsional &amp; Perampasan Prong:</b> Melompati petak yang terisi menjadikan penangkapan bersifat <b>opsional</b>: tambahkan <code>x</code> (mis. <code>44ab-66x</code>) untuk menangkap <b>seluruh Pod dalam tumpukan yang dilompati</b> (penangkapan seluruh tumpukan&mdash;penangkapan sebagian tumpukan tidak pernah diizinkan), atau hilangkan <code>x</code> (mis. <code>44ab-66</code>) untuk membiarkan tumpukan yang dilompati tetap utuh. Selama langkah tunggal atau komposit reguler, tumpukan yang ditangkap diangkat pada akhir giliran ke kumpulan <code>capturedPods</code> pemilik masing-masing Pod, sementara <b>semua Prong yang menempel padanya berpindah ke persediaan Prong pemain yang menangkap</b>. Pod kawan/sekutu dapat dilompati tanpa harus ditangkap.",
            "Gambar 4: Pod A44ab melangkah 1 petak ke Utara (44ab-45) dibandingkan melompat 2 petak ke Timur Laut melewati B55e dengan pilihan menangkap seluruh tumpukan (44ab-66x).",
            "Gambar 5: Rantai lompatan beruntun 33abd-55-57x-75x melompati kawan A44h, berbelok ke Utara menangkap B56e, lalu ke Tenggara menangkap B66c.",
            "Gambar 7: Menempatkan Pod Cadangan ke Markas Utama kosong 33 dibandingkan membebaskan Pod sendiri yang tertawan (53* atau 57*) pada 2P_3B* dan 4P* setelah petak OCTI lawan (atau netral 4P) diduduki.",
            "Bab 3 &mdash; Tumpukan, Langkah Komposit &amp; Kasus Khusus Lampiran A",
            "<b>Penumpukan Pod Sendiri &amp; Langkah Komposit Multi-Pod (<code>,</code>):</b> Beberapa Pod dari warna Anda sendiri boleh bertumpuk di petak yang sama (dilarang pada <code>OFK</code>; pada <code>4P</code>, bertumpuk dengan warna lain&mdash;termasuk rekan diagonal Anda&mdash;sama sekali tidak diperbolehkan). Sebagian atau seluruh Pod milik Anda sendiri yang <b>mengawali giliran di petak yang sama</b> dapat bergerak bersama atau memecah diri melalui jalur langkah 1 petak atau lompatan terpisah dalam satu giliran, dihubungkan tanda koma (mis. <code>44a-45,44beg-34</code>). Penempatan Pod dan pemasangan Prong tidak pernah digabungkan dengan pergerakan.",
            "<b>Penangkapan Seluruh Tumpukan &amp; Pengangkatan Akhir Giliran:</b> Melompati petak terisi dengan <code>x</code> menangkap <b>setiap Pod dalam tumpukan petak tersebut</b>. Pada langkah tunggal maupun komposit reguler, tumpukan yang dilompati dengan <code>x</code> tetap berada secara fisik di papan sebagai tumpuan lompatan bagi rekan setumpukan lainnya selama giliran yang sama dan diangkat bersama-sama pada akhir giliran. Satu Pod <b>tidak pernah boleh melompati petak yang persis sama dua kali</b> dalam satu rantai lompatan.",
            "<b>Lampiran A.1 &mdash; Pengosongan Petak Awal, Lompatan Melingkar Kembali &amp; Larangan Melompati Rekan Setumpukan:</b> Selama rantai lompatannya berlangsung, Pod yang melompat <b>tidak boleh melompati petak awalnya <code>S</code> maupun Pod mana pun yang mengawali giliran bertumpuk bersamanya</b> (sehingga lompatan tembus tepi Tanpa Tepi <code>_ED</code> seperti <code>44bcd-66-86-16-34-54</code> melewati <code>A44a</code> yang diam di <code>44</code> adalah tidak sah). Selain itu, Pod yang melompat boleh <b>mendarat kembali di <code>S</code></b> pada akhir putaran lompatan beruntun <b>hanya apabila seluruh Pod yang mengawali giliran di <code>S</code> mengosongkan <code>S</code></b> selama langkah tersebut (misalnya, pengosongan komposit <code>44a-45,44beg-66x-64x-44</code> sah karena <code>44</code> dikosongkan sepenuhnya, sedangkan langkah tunggal <code>44beg-66x-64x-44</code> yang menyisakan <code>A44a</code> diam di <code>44</code> tidak sah). Lompatan beruntun melingkar tanpa penangkapan (<code>S &rarr; ... &rarr; S</code> saat <code>S</code> dikosongkan sepenuhnya) tetap sah dan dihitung sebagai langkah pergerakan murni (<code>+1</code> menuju batas seri tanpa kemajuan).",
            "<b>Cakupan &amp; Syarat Pembebasan Pod (<code>2P_3B*</code> &amp; <code>4P*</code>):</b> Membebaskan Pod tertawan (<code>xy*</code>) hanya berlaku pada pertandingan <b>3 Markas (<code>2P_3B*</code>) dan 4 Pemain (<code>4P*</code>)</b> (tidak pernah pada penentuan mendadak 1 Markas maupun <code>OFK</code>), selalu membebaskan 1 Pod tertawan <b>milik Anda sendiri</b> (<code>capturedPods &gt; 0</code>), dan selalu mensyaratkan petak OCTI tujuan <b>sudah diduduki oleh sedikitnya satu Pod milik Anda sendiri</b> (tidak boleh petak kosong atau petak yang hanya berisi Pod rekan). Pada <code>2P_3B*</code>, membebaskan ke Markas Utama sendiri juga mensyaratkan Anda menduduki sedikitnya satu markas lawan; membebaskan langsung ke markas lawan yang sudah Anda duduki hanya memerlukan <code>capturedPods &gt; 0</code>. Pada <code>4P*</code>, membebaskan ke markas sudut milik sendiri atau milik rekan mensyaratkan Anda secara pribadi menduduki petak OCTI netral (<code>53</code>/<code>57</code>) atau lawan (walaupun rekan Anda telah tereliminasi, <code>xy*</code> di markas sudutnya hanya membebaskan Pod milik Anda sendiri dan tidak pernah memulihkan rekan yang tereliminasi).",
            "<b>Tembus Tepi Torus Tanpa Tepi (<code>_ED</code>):</b> Kolom <code>1</code> tersambung ke kolom <code>9</code> dan baris <code>1</code> tersambung ke baris <code>9</code> (termasuk arah diagonal) baik untuk langkah 1 petak (<code>15g-95</code>) maupun lompatan 2 petak (<code>97b-29x</code> melompati <code>B18e</code>).",
            "<b>SuperProng &amp; Langkah Awal Bonus (<code>_SP</code>, <code>;</code>):</b> SuperProng emas (<code>!</code>) bersifat permanen begitu dipasang. Sebelum aksi giliran reguler, Anda dapat menjalankan <b>1 langkah awal bonus untuk setiap SuperProng yang Anda kendalikan di papan</b> (hingga <b>2</b> pada <code>2P_SP</code> dan hingga <b>4</b> pada <code>4P_SP</code> apabila SuperProng lawan ditangkap), dipisahkan tanda titik koma (mis. <code>44ab!-66x;66ab!-67</code>). Setiap langkah awal menggerakkan 1 Pod secara ketat dalam <b>garis lurus</b> (langkah 1 petak atau rantai lompatan segaris, tanpa berbelok) mengikuti satu soket SuperProng berbeda yang belum menjalankan langkah awal bonus pada giliran ini. Tumpukan yang dilompati dengan <code>x</code> selama langkah awal bonus SuperProng <b>diangkat seketika</b> setelah langkah awal tersebut sebelum langkah awal SuperProng berikutnya atau aksi reguler dimulai! Pod yang bergerak pada langkah awal bonus tetap dapat menggunakan <b>Prong mana pun</b> miliknya pada tahap giliran reguler, dan <b>kemenangan hanya diperiksa saat seluruh giliran selesai sepenuhnya</b>.",
            "Gambar 6: Tumpukan [A44a, A44beg] memecah diri melalui 44a-45,44beg-34 atau mengosongkan 44 sepenuhnya melalui 44a-45,44beg-66x-64x-44 sehingga A44beg mendarat secara sah di petak 44 yang telah kosong (Lampiran A.1).",
            "Gambar 8: Langkah 1 petak tembus tepi 15g-95 pada papan Tanpa Tepi (_ED) dan tangkapan lompatan diagonal tembus tepi 97b-29x melintasi batas papan.",
            "Gambar 9: Langkah awal bonus garis lurus SuperProng emas 44ab!-66x; (1 langkah awal bonus per SuperProng yang dikendalikan di papan) yang langsung mengangkat B55e! sebelum langkah 1 petak reguler 66ab!-67, dengan kemenangan diperiksa pada akhir giliran penuh.",
            "Bab 4 &mdash; Kondisi Kemenangan, Eliminasi, Terkunci &amp; Seri Tanpa Kemajuan",
            "<b>Keluarga <code>2P_3B</code> (Standar 3 Markas):</b> Menang pada akhir giliran penuh Anda apabila Pod Anda secara bersamaan menduduki <b>ketiga</b> Markas Utama lawan (<code>37</code>, <code>57</code>, <code>77</code> untuk A; <code>33</code>, <code>53</code>, <code>73</code> untuk B).",
            "<b>Keluarga <code>2P_1B</code> (Kilat 1 Markas):</b> Menang pada akhir giliran penuh Anda ketika Pod Anda menduduki <b>1 mana pun</b> dari 3 Markas Utama lawan.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Menang pada akhir giliran penuh Anda ketika Pod Anda menduduki <b>1 mana pun</b> dari 4 Markas Utama lawan (<code>26, 36, 46, 56</code> untuk A; <code>22, 32, 42, 52</code> untuk B).",
            "<b>Keluarga Kemitraan <code>4P</code> (Tim A&amp;C vs. Tim B&amp;D):</b> Pasangan diagonal menang bersama pada akhir giliran penuh ketika Pod sekutu secara bersamaan menduduki <b>kedua markas sudut lawan</b> (<code>37</code> &amp; <code>73</code> untuk Tim A&amp;C; <code>33</code> &amp; <code>77</code> untuk Tim B&amp;D).",
            "<b>Eliminasi Permanen Pendudukan Markas 4 Pemain &amp; Markas Rekan yang Aman:</b> Pada <code>4P</code>, jika Pod <b>lawan</b> mengakhiri giliran penuhnya di Markas Utama sudut milik Anda (atau seluruh Pod Anda tertangkap dengan nol Pod Cadangan tersisa), Anda <b>tereliminasi secara permanen</b> hingga sisa pertandingan: seluruh sisa Pod milik Anda di papan disapu ke kumpulan <code>capturedPods</code> Anda, dan <b>semua Prong (biasa maupun SuperProng) dari Pod milik Anda yang disapu berpindah kepada pemain lawan yang menduduki markas Anda</b>. Kursi yang tereliminasi dilewati dalam rotasi giliran sementara rekan yang masih bertahan terus bermain. Sebaliknya, <b>melangkah atau melompat ke markas awal rekan diagonal Anda yang kosong (atau yang sudah ditempati Pod milik Anda sendiri) adalah sah dan tanpa konsekuensi apa pun&mdash;hal itu tidak pernah mengeliminasi rekan Anda dari pertandingan</b> (apabila rekan Anda untuk sementara menempati markas awal Anda saat Anda memiliki nol Pod di papan, giliran Anda cukup dilewati hingga rekan Anda mengosongkannya).",
            "<b>Tangkapan Total / Terkunci:</b> Karena pemindahan Prong dilarang, pemain yang memiliki 0 Pod Cadangan dan 0 Prong cadangan sementara seluruh Pod miliknya di papan terkurung rapat tidak memiliki <b>langkah sah satu pun</b>. Jika pemain berikutnya pada <code>2P</code> / <code>OFK</code> (atau kedua kursi lawan pada <code>4P</code>) tidak memiliki langkah sah, pemain atau tim yang baru saja melangkah langsung menang (pada <code>4P</code>, pemain yang terkunci namun rekannya masih memiliki langkah sah hanya dilewati gilirannya). Pertandingan berwaktu juga berakhir karena waktu habis atau menyerah.",
            "<b>Seri Tanpa Kemajuan di Semua Varian (<code>NO_PROGRESS_DRAW</code>, <code>noProgressPlies</code>):</b> Pertandingan berakhir otomatis dengan <b>seri (<code>NO_PROGRESS_DRAW</code>)</b> apabila <b><code>30</code> langkah berturut-turut (<code>15</code> giliran penuh per pemain)</b> pada varian <code>2P*</code> atau <code>OFK</code> mana pun&mdash;atau <b><code>60</code> langkah berturut-turut (<code>15</code> putaran penuh)</b> pada varian <code>4P*</code> mana pun&mdash;selesai tanpa satu pun <b>kejadian kemajuan</b>: penangkapan Pod (<code>x</code>, termasuk langkah awal bonus <code>_SP</code>), penempatan Pod Cadangan (<code>xy</code>) atau pembebasan (<code>xy*</code>), pemasangan Prong/SuperProng (<code>+d</code>/<code>+d!</code>), maupun eliminasi permanen pemain <code>4P</code>. Dievaluasi pada akhir giliran <b>setelah</b> pemeriksaan kemenangan pendudukan markas, tangkapan total, dan lawan terkunci tanpa langkah sah (sehingga mencapai kemenangan pada langkah ke-<code>30</code>/<code>60</code> tetap dihitung menang).",
            "Gambar 10: Kemitraan 4 Pemain (4P): sekutu diagonal A(33)&C(77) vs. B(37)&D(73), tumpukan sewarna [A55ab, A55f], Pod rekan C65cf, masuk aman ke markas rekan, serta petak netral 53/57.",
            "Gambar 11: OCTI for Kids (OFK) pada kisi 6x7: 4 markas per sisi, 0 Pod Cadangan, Prong permanen, tanpa tumpukan, kemenangan 1 markas (34a-36x).",
            "Gambar 12: Keadaan menang 2P_3B (status = O) saat Pemain A menduduki ketiga markas lawan secara bersamaan (A37c, A57a, A77g).",
            "Bab 5 &mdash; Matriks Referensi 13 Varian Resmi",
            "Setiap pertandingan di Web, Desktop, Android, Arsip, dan SDK Robot menggunakan salah satu dari <b>13 kode varian resmi</b>:",
            "Kode", "Kisi", "Pemain", "Pod (Awal+Cad)", "Prong (B/SP)", "Aturan Khusus &amp; Target Menang", "Berperingkat",
            "Ya", "Santai",
            "Aturan standar; duduki ketiga markas lawan",
            "SuperProng <code>!</code> + <code>;</code> langkah awal bonus lurus (&le;2); 3 markas",
            "Tembus tepi torus Tanpa Tepi; 3 markas",
            "SuperProng (&le;2 langkah awal) + Tanpa Tepi; 3 markas",
            "Kilat penentuan mendadak; duduki 1 markas lawan mana pun",
            "SuperProng (&le;2 langkah awal) + penentuan mendadak 1 markas",
            "Tanpa Tepi + penentuan mendadak 1 markas",
            "SuperProng (&le;2 langkah awal) + Tanpa Tepi + 1 markas",
            "Petak netral <code>53/57</code>, eliminasi permanen jika markas diduduki; 2 markas lawan",
            "Kemitraan 4P + langkah awal bonus lurus SuperProng (&le;4)",
            "Kemitraan 4P + tembus tepi Tanpa Tepi",
            "Kemitraan 4P + SuperProng (&le;4 langkah awal) + Tanpa Tepi",
            "4 markas per sisi, 0 cadangan, tanpa tumpukan maupun langkah komposit; cukup 1 markas untuk menang",
            "Bab 6 &mdash; Notasi Langkah Resmi &amp; Spesifikasi OCTI-FEN",
            "Tata Bahasa Notasi Langkah",
            "Sintaks", "Aksi", "Contoh",
            "Tempatkan Pod Cadangan ke petak OCTI",
            "Bebaskan Pod sendiri yang tertawan ke petak OCTI yang sudah diduduki (khusus <code>2P_3B*</code> &amp; <code>4P*</code>)",
            "Pasang Prong biasa pada soket <code>d &isin; {a..h}</code>",
            "Pasang SuperProng emas pada soket <code>d</code>",
            "Langkah 1 petak tanpa menangkap",
            "Rantai lompatan tunggal atau beruntun (<code>x</code> = tangkap seluruh tumpukan opsional)",
            "Langkah komposit yang memecah Pod dari petak asal yang sama",
            "1&ndash;4 langkah awal bonus lurus SuperProng (1 per SP di papan; pengangkatan <code>x</code> seketika) + giliran reguler",
            "Format String Posisi OCTI-FEN",
            "Keadaan papan diserialisasi menjadi string <b>OCTI-FEN</b> yang dipisahkan titik dua (11 ruas untuk <code>2</code> / <code>OFK</code>; 13 ruas untuk <code>4</code>):",
            "<code>mode</code>: <code>2</code>, <code>4</code>, atau <code>OFK</code> &bull; <code>bases</code>: <code>3</code> atau <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> atau <code>1</code>.",
            "<code>status</code>: <code>N</code> (belum mulai), <code>P</code> (berlangsung), <code>O</code> (permainan berakhir), <code>R</code> (menyerah).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code>, atau <code>-</code> &bull; <code>elimMask</code>: bitmask eliminasi 4P (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (mis. <code>4,0,25,0</code>).",
            "<code>squares</code>: daftar Pod dipisahkan koma dalam urutan per baris <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (mis. <code>A44ab!,B55e!</code>), atau <code>-</code> jika kosong.",
            "<code>noProgressPlies</code> (ruas akhir opsional ke-12/ke-14): langkah berturut-turut tanpa kemajuan (<code>0..1000</code>, bawaan <code>0</code> bila dihilangkan; fungsi kanonis <code>toPositionString()</code> menghasilkan string 11/13 ruas).",
            "Bab 7 &mdash; Aturan Elo Dua Kelompok &amp; Arena Kompetitif",
            "Peringkat resmi dimulai dari <b>1200 Elo</b> pada tiga kategori peringkat varian 1 lawan 1 independen: <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b>, dan <b><code>OFK</code></b>. Semua pertandingan 4 Pemain (<code>4P*</code>), SuperProng (<code>*_SP</code>), Tanpa Tepi (<code>*_ED</code>), varian gabungan (<code>*_SPED</code>), serta luring bersifat santai (tanpa peringkat).",
            "<b>Kelompok 1 &mdash; Peringkat Khusus Manusia (Papan Peringkat &amp; Daftar Pemain):</b><br>Mencatat khusus pertandingan daring berperingkat Manusia lawan Manusia dengan faktor Elo standar <code>K = 32</code>. Peringkat Khusus Manusia sama sekali tidak dipengaruhi oleh pertandingan robot.",
            "<b>Kelompok 2 &mdash; Arena Terbuka Robot &amp; Manusia (Papan Peringkat &amp; Direktori Robot):</b><br>Mencatat pertandingan daring berperingkat yang melibatkan sedikitnya satu robot AI terdaftar (Manusia lawan Robot dan Robot lawan Robot) dengan tiga pengaman anti-manipulasi peringkat:<ul><li><b>Pertandingan Manusia lawan Robot:</b> Pertandingan berperingkat tanpa batas dengan standar <code>K = 32</code> (termasuk melawan robot milik Anda sendiri).</li><li><b>Faktor K Robot lawan Robot:</b> Pertandingan antar pemilik berbeda memakai standar <code>K = 32</code>; pengujian mandiri oleh pemilik yang sama memakai nilai tereduksi <code>K = 16</code>.</li><li><b>Batas Pasangan Robot lawan Robot &amp; Pengaman Menyerah Dini:</b> Pertandingan Robot lawan Robot dibatasi maksimal <b>4 pertandingan berperingkat per jendela bergulir 24 jam</b> untuk setiap pasangan robot tak terurut (pertandingan ke-5 dan seterusnya otomatis menjadi santai; tidak ada batas harian terhadap robot lawan yang berbeda), dan menyerah sebelum <b>8 langkah</b> mencatat perubahan 0 Elo.</li></ul>"
        },
        /* 15. Vietnamese (vi) */
        {
            "vi",
            {
                { "ch1", "Chương 1", "Hình học bàn cờ, Pod, Prong & bố trí ban đầu", "Tọa độ 9x9, Pod dự bị ban đầu và 8 lỗ cắm hướng la bàn a-h" },
                { "ch2", "Chương 2", "Các hành động trong lượt", "Cắm Prong, bước 1 ô, nhảy liên tiếp, đặt Pod dự bị & giải cứu" },
                { "ch3", "Chương 3", "Chồng quân, nước đi tổ hợp & Phụ lục A", "Nước đi tổ hợp dấu phẩy, nhảy vòng A.1, Không biên & SuperProng" },
                { "ch4", "Chương 4", "Điều kiện thắng & loại người chơi 4P", "Chiếm 3 căn cứ vs 1 căn cứ, loại 4P, phong tỏa & hòa sau 30/60 nước đi" },
                { "ch5", "Chương 5", "Bảng tham chiếu 13 biến thể chuẩn", "Kích thước lưới, số Pod/Prong, quy tắc bổ sung & điều kiện tính Elo" },
                { "ch6", "Chương 6", "Ký hiệu nước đi & cú pháp OCTI-FEN", "Cú pháp ký hiệu đại số chính thức và định dạng OCTI-FEN 11/13 trường" },
                { "ch7", "Chương 7", "Hệ thống Elo hai nhóm & luật đấu trường", "Nhóm 1 dành riêng cho người chơi vs Nhóm 2 đấu trường mở Robot & người" }
            },
            "Sách luật minh họa chính thức OCTI",
            "Toàn bộ luật chơi đa biến thể, cơ chế chiến thuật, các tình huống đặc biệt ở Phụ lục A, cú pháp nước đi/OCTI-FEN chính thức và quy định thi đấu Elo hai nhóm.",
            "Mục lục:",
            "Chương 1 &mdash; Hình học bàn cờ, Pod, Prong &amp; bố trí ban đầu",
            "OCTI là trò chơi chiến thuật trừu tượng tất định, thông tin hoàn hảo về kiến tạo hướng di chuyển. Mỗi quân Pod được đặt vào bàn cờ ở trạng thái trống (0 hướng di chuyển) và dần có khả năng cơ động theo hướng khi người chơi cắm thêm <b>Prong</b> qua từng lượt.",
            "Pod (quân cờ hình bát giác chính có 8 lỗ cắm hướng xung quanh)",
            "Prong (chốt hướng cắm vào lỗ của Pod để mở hướng di chuyển)",
            "SuperProng (chốt vàng đặc biệt cho phép đi một pha mở đầu thưởng theo đường thẳng)",
            "<b>Lưới tiêu chuẩn 9&times;9:</b> Các cột <code>x = 1..9</code> chạy từ Tây sang Đông (trái sang phải); các hàng <code>y = 1..9</code> chạy từ Nam lên Bắc (dưới lên trên). Mỗi ô được ký hiệu bằng hai chữ số <code>xy</code> (ví dụ: <code>53</code> là cột 5, hàng 3).",
            "<b>Tài nguyên ban đầu cho 2 người chơi (<code>2P_3B</code> &amp; <code>2P_1B</code>):</b> Người chơi A bắt đầu với <b>3 Pod</b> trên các căn cứ chính OCTI <code>33</code>, <code>53</code> và <code>73</code>; Người chơi B bắt đầu với <b>3 Pod</b> trên các căn cứ chính OCTI <code>37</code>, <code>57</code> và <code>77</code>. Mỗi bên còn giữ <b>4 Pod dự bị</b> (tổng cộng 7 Pod mỗi người chơi) và <b>25 Prong</b> (hoặc <b>24 Prong thường + 1 SuperProng vàng</b> trong các biến thể <code>_SP</code>).",
            "<b>8 lỗ cắm Prong theo la bàn (<code>a</code>&ndash;<code>h</code>):</b> Mỗi Pod bát giác có 8 lỗ cắm quanh viền được đánh dấu theo chiều kim đồng hồ tính từ hướng Bắc: <code>a</code>=Bắc (<code>0,+1</code>), <code>b</code>=Đông Bắc (<code>+1,+1</code>), <code>c</code>=Đông (<code>+1,0</code>), <code>d</code>=Đông Nam (<code>+1,-1</code>), <code>e</code>=Nam (<code>0,-1</code>), <code>f</code>=Tây Nam (<code>-1,-1</code>), <code>g</code>=Tây (<code>-1,0</code>) và <code>h</code>=Tây Bắc (<code>-1,+1</code>).",
            "Hình 1: Bố trí tiêu chuẩn 9x9 cho 2 người chơi (2P_3B) với hàng căn cứ chính 33/53/73 đối đầu 37/57/77.",
            "Hình 2: 2P_1B (Tốc chiến 1 căn cứ) dùng cùng bố trí ban đầu, nhưng chỉ cần chiếm giữ bất kỳ 1 căn cứ đối phương nào (55ab-57x) là đủ để giành chiến thắng.",
            "Hình 3: 8 lỗ cắm la bàn a-h quanh Pod A55acg và thao tác cắm Prong từ khay sẵn có (55acg+b) vào lỗ cắm trống.",
            "Chương 2 &mdash; Các hành động trong lượt",
            "Người chơi A đi trước và các lượt luân phiên nghiêm ngặt (trong <code>4P</code>, thứ tự lượt xoay vòng <code>A &rarr; B &rarr; C &rarr; D</code>, bỏ qua các ghế đã bị loại). Cấm bỏ lượt; đến lượt của bạn, bạn bắt buộc phải thực hiện <b>đúng một</b> hành động chính:",
            "<b>Cắm một Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Lấy 1 Prong từ kho Prong sẵn có của bạn và cắm vào một lỗ cắm trống trên một Pod của bạn đang ở trên bàn cờ (ví dụ: <code>55acg+b</code>; hoặc <code>33+b!</code> cho SuperProng vàng). Một khi đã cắm, Prong gắn cố định vĩnh viễn trên Pod đó&mdash;việc rút Prong đã gắn sang lỗ cắm khác bị cấm tuyệt đối trong mọi biến thể.",
            "<b>Bước 1 ô (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Di chuyển một Pod đi 1 ô dọc theo một hướng Prong đã gắn trên nó tới ô trống hoặc chồng quân gồm các Pod của chính bạn (ví dụ: <code>44ab-45</code>). Bước 1 ô <b>không bao giờ bắt quân</b> và không thể đi vào ô đang có Pod của người chơi khác (kể cả đồng đội chéo góc của bạn trong <code>4P</code>).",
            "<b>Nhảy 2 ô &amp; chuỗi nhảy liên tiếp (<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> Nhảy 2 ô dọc theo một hướng Prong đã gắn vượt qua một ô liền kề đang có quân (của phe mình/đồng đội hoặc đối phương) để đáp xuống ô trống ngay phía sau. Bạn có thể nối tiếp các cú nhảy bổ sung bằng chính Pod đó dọc theo bất kỳ Prong nào đã gắn trên nó (được phép đổi hướng giữa chuỗi) và dừng sớm sau bất kỳ cú nhảy hợp lệ nào.",
            "<b>Đặt Pod dự bị (<code>xy</code>) hoặc giải cứu Pod bị bắt (<code>xy*</code>):</b> Đặt 1 Pod dự bị lên căn cứ chính của riêng bạn khi ô đó đang trống hoặc chỉ có Pod của chính bạn (hoặc lên một căn cứ đối phương mà bạn đang chiếm giữ), hoặc&mdash;trong các ván 3 căn cứ (<code>2P_3B*</code>) và 4 người chơi (<code>4P*</code>)&mdash;giải cứu 1 Pod bị bắt của chính bạn (<code>xy*</code>) lên một ô OCTI mà bạn đang chiếm giữ khi đủ điều kiện (quy tắc giải cứu không áp dụng cho biến thể quyết thắng nhanh 1 căn cứ hay <code>OFK</code>).",
            "<b>Bắt trọn chồng quân tùy chọn &amp; thu giữ Prong:</b> Khi nhảy qua một ô có quân, việc bắt quân là <b>tùy chọn</b> đối với mọi chồng quân bị nhảy qua: thêm <code>x</code> (ví dụ: <code>44ab-66x</code>) để bắt <b>toàn bộ các Pod trong chồng quân bị nhảy qua</b> (bắt trọn chồng quân&mdash;không bao giờ được phép bắt lẻ một phần chồng quân), hoặc lược bỏ <code>x</code> (ví dụ: <code>44ab-66</code>) để giữ nguyên chồng quân đó. Trong các nước đi đơn hoặc nước đi tổ hợp thông thường, các chồng quân bị bắt được nhấc khỏi bàn cờ vào cuối lượt để đưa vào nhóm Pod bị bắt (<code>capturedPods</code>) của chủ sở hữu từng Pod, đồng thời <b>toàn bộ Prong gắn trên chúng được chuyển vào kho Prong sẵn có của người bắt quân</b>.",
            "Hình 4: Pod A44ab bước 1 ô lên hướng Bắc (44ab-45) so với nhảy 2 ô hướng Đông Bắc qua B55e kèm bắt trọn chồng quân tùy chọn (44ab-66x).",
            "Hình 5: Chuỗi nhảy liên tiếp 33abd-55-57x-75x nhảy qua Pod phe mình A44h, đổi hướng lên Bắc để bắt B56e, rồi đổi hướng sang Đông Nam để bắt B66c.",
            "Hình 7: Đặt Pod dự bị lên căn cứ chính trống 33 so với giải cứu Pod bị bắt của chính bạn (53* hoặc 57*) trong 2P_3B* và 4P* sau khi đã chiếm giữ ô OCTI đối phương (hoặc trung lập 4P).",
            "Chương 3 &mdash; Chồng quân, nước đi tổ hợp &amp; các tình huống Phụ lục A",
            "<b>Chồng quân của chính bạn &amp; nước đi tổ hợp đa Pod (<code>,</code>):</b> Nhiều Pod thuộc màu của chính bạn được phép xếp chồng trên cùng một ô (bị cấm trong <code>OFK</code>; trong <code>4P</code>, việc xếp chồng với màu khác&mdash;kể cả đồng đội chéo góc của bạn&mdash;không bao giờ được phép). Bất kỳ số lượng nào trong các Pod của chính bạn <b>bắt đầu lượt trên cùng một ô</b> đều có thể di chuyển cùng nhau hoặc tách ra theo các đường bước/nhảy độc lập trong một lượt, nối với nhau bằng dấu phẩy (ví dụ: <code>44a-45,44beg-34</code>). Đặt Pod và cắm Prong không bao giờ kết hợp cùng di chuyển.",
            "<b>Bắt trọn chồng quân &amp; nhấc quân cuối lượt:</b> Nhảy qua một ô có quân kèm <code>x</code> sẽ bắt <b>mọi Pod trong chồng quân tại ô đó</b>. Trong các nước đi đơn và nước đi tổ hợp thông thường, những chồng quân bị nhảy kèm <code>x</code> vẫn nằm lại trên bàn cờ làm bệ đỡ nhảy cho các Pod cùng chồng còn lại trong lượt đó và được nhấc khỏi bàn cùng lúc khi kết thúc lượt. Một Pod <b>không bao giờ được nhảy qua đúng cùng một ô hai lần</b> trong một chuỗi nhảy.",
            "<b>Phụ lục A.1 &mdash; Làm trống ô xuất phát, nhảy vòng quay lại &amp; cấm nhảy qua Pod cùng chồng:</b> Trong suốt chuỗi nhảy của mình, một Pod đang nhảy <b>không được nhảy qua ô xuất phát <code>S</code> của chính nó hoặc qua bất kỳ Pod nào đã xếp cùng chồng với nó ở đầu lượt</b> (do đó cú nhảy xuyên biên Không biên <code>_ED</code> như <code>44bcd-66-86-16-34-54</code> nhảy qua <code>A44a</code> đang đứng yên tại <code>44</code> là phạm luật). Hơn nữa, một Pod đang nhảy chỉ được <b>đáp trở lại ô <code>S</code></b> ở cuối vòng nhảy liên tiếp <b>khi tất cả các Pod bắt đầu lượt trên <code>S</code> đều rời hết khỏi <code>S</code></b> trong nước đi đó (ví dụ: nước đi tổ hợp làm trống sạch ô <code>44a-45,44beg-66x-64x-44</code> là hợp lệ vì ô <code>44</code> đã trống hoàn toàn, trong khi nước đi đơn <code>44beg-66x-64x-44</code> để lại <code>A44a</code> đứng yên trên <code>44</code> là phạm luật). Các chuỗi nhảy vòng khép kín không bắt quân (<code>S &rarr; ... &rarr; S</code> khi <code>S</code> được làm trống hoàn toàn) vẫn hợp lệ và được tính là nước đi di chuyển thuần túy (<code>+1</code> vào ngưỡng hòa không tiến triển).",
            "<b>Phạm vi &amp; điều kiện giải cứu Pod (<code>2P_3B*</code> &amp; <code>4P*</code>):</b> Giải cứu Pod bị bắt (<code>xy*</code>) chỉ áp dụng riêng cho các ván <b>3 căn cứ (<code>2P_3B*</code>) và 4 người chơi (<code>4P*</code>)</b> (không bao giờ áp dụng cho quyết thắng nhanh 1 căn cứ hay <code>OFK</code>), luôn giải cứu 1 Pod bị bắt <b>của chính bạn</b> (<code>capturedPods &gt; 0</code>), và luôn yêu cầu ô OCTI đích <b>đang có sẵn ít nhất một Pod của chính bạn</b> (không bao giờ được là ô trống hay ô chỉ có Pod đồng đội). Trong <code>2P_3B*</code>, giải cứu về căn cứ chính của bạn đòi hỏi bạn phải đang chiếm giữ ít nhất một căn cứ đối phương; giải cứu trực tiếp lên căn cứ đối phương mà bạn đang chiếm giữ chỉ cần <code>capturedPods &gt; 0</code>. Trong <code>4P*</code>, giải cứu lên căn cứ góc của bạn hoặc của đồng đội đòi hỏi cá nhân bạn đang chiếm giữ một ô OCTI trung lập (<code>53</code>/<code>57</code>) hoặc của đối phương (ngay cả khi đồng đội đã bị loại, <code>xy*</code> trên căn cứ góc của họ vẫn chỉ giải cứu Pod của bạn và không bao giờ khôi phục đồng đội đã bị loại).",
            "<b>Cuộn vòng hình xuyến Không biên (<code>_ED</code>):</b> Cột <code>1</code> nối vòng sang cột <code>9</code> và hàng <code>1</code> nối vòng sang hàng <code>9</code> (kể cả đường chéo) đối với cả bước 1 ô (<code>15g-95</code>) lẫn nhảy 2 ô (<code>97b-29x</code> nhảy qua <code>B18e</code>).",
            "<b>SuperProng &amp; di chuyển mở đầu thưởng (<code>_SP</code>, <code>;</code>):</b> SuperProng vàng (<code>!</code>) gắn cố định vĩnh viễn sau khi cắm. Trước hành động chính của lượt, bạn có thể thực hiện <b>1 pha di chuyển mở đầu thưởng cho mỗi SuperProng bạn điều khiển trên bàn cờ</b> (tối đa <b>2</b> trong <code>2P_SP</code> và tối đa <b>4</b> trong <code>4P_SP</code> khi bắt được SuperProng đối phương), ngăn cách bằng dấu chấm phẩy (ví dụ: <code>44ab!-66x;66ab!-67</code>). Mỗi pha mở đầu chỉ di chuyển 1 Pod nghiêm ngặt theo <b>một đường thẳng</b> (bước 1 ô hoặc chuỗi nhảy thẳng hàng, không đổi hướng) dọc theo một lỗ cắm SuperProng riêng biệt chưa kích hoạt mở đầu thưởng trong lượt này. Các chồng quân bị nhảy kèm <code>x</code> trong pha mở đầu thưởng SuperProng sẽ <b>bị nhấc khỏi bàn ngay lập tức</b> sau pha mở đầu đó trước khi pha mở đầu SuperProng tiếp theo hoặc hành động chính bắt đầu! Một Pod đã di chuyển trong pha mở đầu thưởng vẫn được dùng <b>bất kỳ</b> Prong nào của nó trong pha hành động chính, và <b>điều kiện chiến thắng chỉ được kiểm tra khi hoàn tất trọn vẹn cả lượt</b>.",
            "Hình 6: Chồng quân [A44a, A44beg] tách ra qua 44a-45,44beg-34 hoặc làm trống hoàn toàn ô 44 qua 44a-45,44beg-66x-64x-44 để A44beg đáp hợp lệ trở lại ô 44 đã trống (Phụ lục A.1).",
            "Hình 8: Nước bước xuyên biên Không biên (_ED) 15g-95 và cú nhảy chéo bắt quân xuyên biên 97b-29x qua đường biên nối thông.",
            "Hình 9: Pha di chuyển mở đầu thưởng đường thẳng bằng SuperProng vàng 44ab!-66x; (1 lần di chuyển thưởng cho mỗi SuperProng điều khiển trên bàn) loại bỏ ngay B55e! trước bước đi chính 66ab!-67, kiểm tra chiến thắng khi hoàn tất trọn lượt.",
            "Chương 4 &mdash; Điều kiện thắng, loại trừ, phong tỏa &amp; hòa không tiến triển",
            "<b>Nhóm <code>2P_3B</code> (Tiêu chuẩn 3 căn cứ):</b> Thắng khi kết thúc trọn lượt của bạn nếu các Pod của bạn đồng thời chiếm giữ <b>cả 3</b> căn cứ chính của đối phương (<code>37</code>, <code>57</code>, <code>77</code> đối với A; <code>33</code>, <code>53</code>, <code>73</code> đối với B).",
            "<b>Nhóm <code>2P_1B</code> (Tốc chiến 1 căn cứ):</b> Thắng khi kết thúc trọn lượt của bạn nếu Pod của bạn chiếm giữ <b>bất kỳ 1</b> trong 3 căn cứ chính của đối phương.",
            "<b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Thắng khi kết thúc trọn lượt của bạn nếu Pod của bạn chiếm giữ <b>bất kỳ 1</b> trong 4 căn cứ chính của đối phương (<code>26, 36, 46, 56</code> đối với A; <code>22, 32, 42, 52</code> đối với B).",
            "<b>Nhóm đồng đội <code>4P</code> (Đội A&amp;C đấu Đội B&amp;D):</b> Hai đồng đội chéo góc cùng thắng khi kết thúc trọn lượt nếu các Pod liên minh đồng thời chiếm giữ <b>cả hai căn cứ góc của đối phương</b> (<code>37</code> &amp; <code>73</code> đối với Đội A&amp;C; <code>33</code> &amp; <code>77</code> đối với Đội B&amp;D).",
            "<b>Loại vĩnh viễn khi mất căn cứ trong 4 người chơi &amp; căn cứ đồng đội an toàn:</b> Trong <code>4P</code>, nếu một Pod <b>đối phương</b> kết thúc trọn lượt trên căn cứ chính ở góc của bạn (hoặc toàn bộ Pod của bạn đều bị bắt và không còn Pod dự bị nào), bạn bị <b>loại vĩnh viễn</b> trong phần còn lại của ván đấu: toàn bộ các Pod còn lại trên bàn cờ của bạn được đưa vào nhóm Pod bị bắt (<code>capturedPods</code>) của bạn, và <b>tất cả Prong (cả Prong thường lẫn SuperProng) trên các Pod bị quét đó được chuyển cho người chơi đối phương vừa chiếm căn cứ</b>. Các ghế bị loại được bỏ qua trong vòng quay lượt trong khi người đồng đội còn trụ lại tiếp tục thi đấu. Ngược lại, <b>bước hoặc nhảy vào căn cứ xuất phát đang trống của đồng đội chéo góc (hoặc chỉ đang có Pod của chính bạn) là hợp lệ và không gây hậu quả nào&mdash;không bao giờ loại đồng đội của bạn khỏi ván đấu</b> (nếu đồng đội tạm thời đứng trên căn cứ xuất phát của bạn khi bạn không còn Pod nào trên bàn cờ, lượt của bạn chỉ đơn giản bị bỏ qua cho đến khi họ rời khỏi ô đó).",
            "<b>Quét sạch quân / Phong tỏa:</b> Vì cấm đổi lỗ cắm Prong, một người chơi đã hết Pod dự bị và hết Prong sẵn có trong khi các Pod trên bàn cờ đều bị vây kín sẽ có <b>0 nước đi hợp lệ</b>. Nếu người chơi kế tiếp trong <code>2P</code> / <code>OFK</code> (hoặc cả hai ghế đối phương trong <code>4P</code>) có 0 nước đi hợp lệ, người chơi/đội vừa đi nước trước đó thắng ngay lập tức (trong <code>4P</code>, nếu một người chơi bị phong tỏa nhưng đồng đội vẫn còn nước đi hợp lệ thì chỉ lượt của ghế đó bị bỏ qua). Các ván đấu có đồng hồ cũng kết thúc khi hết giờ hoặc đầu hàng.",
            "<b>Luật hòa không tiến triển chung (<code>NO_PROGRESS_DRAW</code>, <code>noProgressPlies</code>):</b> Ván đấu tự động kết thúc <b>hòa (<code>NO_PROGRESS_DRAW</code>)</b> khi <b><code>30</code> nước đi liên tiếp (<code>15</code> lượt đủ mỗi người chơi)</b> trong bất kỳ biến thể <code>2P*</code> hay <code>OFK</code> nào&mdash;hoặc <b><code>60</code> nước đi liên tiếp (<code>15</code> vòng đủ)</b> trong bất kỳ biến thể <code>4P*</code> nào&mdash;hoàn tất mà không có bất kỳ <b>sự kiện tiến triển</b> nào: bắt Pod (<code>x</code>, tính cả các pha mở đầu thưởng <code>_SP</code>), đặt Pod dự bị (<code>xy</code>) hoặc giải cứu (<code>xy*</code>), cắm Prong/SuperProng (<code>+d</code>/<code>+d!</code>), hay loại vĩnh viễn người chơi <code>4P</code>. Điều kiện này được xét khi hoàn tất lượt <b>sau</b> các kiểm tra thắng chiếm căn cứ, quét sạch toàn bộ quân và phong tỏa hết nước đi hợp lệ (nhờ vậy nếu đạt điều kiện thắng ở đúng nước đi thứ <code>30</code>/<code>60</code> thì vẫn được tính thắng).",
            "Hình 10: Đồng đội 4 người chơi (4P): liên minh chéo góc A(33)&C(77) đấu B(37)&D(73), chồng quân cùng màu [A55ab, A55f], Pod của đồng đội C65cf, vào căn cứ đồng đội an toàn và hai ô trung lập 53/57.",
            "Hình 11: OCTI for Kids (OFK) trên lưới 6x7: 4 căn cứ mỗi bên, 0 Pod dự bị, Prong cố định vĩnh viễn, cấm chồng quân và thắng khi chiếm 1 căn cứ (34a-36x).",
            "Hình 12: Thế cờ chiến thắng 2P_3B (status = O) khi Người chơi A đồng thời chiếm giữ cả 3 căn cứ đối phương (A37c, A57a, A77g).",
            "Chương 5 &mdash; Bảng tham chiếu 13 biến thể chuẩn",
            "Mọi ván đấu trên Web, Máy tính, Android, Lưu trữ và SDK Bot đều sử dụng một trong <b>13 mã biến thể chuẩn</b>:",
            "Mã",
            "Lưới",
            "Người chơi",
            "Pod (Đầu+Dự bị)",
            "Prong (Thường/SP)",
            "Quy tắc bổ sung &amp; mục tiêu thắng",
            "Tính Elo",
            "Có",
            "Giao hữu",
            "Luật tiêu chuẩn; chiếm đồng thời cả 3 căn cứ đối phương",
            "SuperProng <code>!</code> + <code>;</code> mở đầu thưởng đi thẳng (&le;2); 3 căn cứ",
            "Cuộn vòng hình xuyến Không biên; 3 căn cứ",
            "SuperProng (&le;2 mở đầu thưởng) + Không biên; 3 căn cứ",
            "Tốc chiến quyết thắng nhanh; chiếm bất kỳ 1 căn cứ đối phương",
            "SuperProng (&le;2 mở đầu thưởng) + quyết thắng nhanh 1 căn cứ",
            "Không biên + quyết thắng nhanh 1 căn cứ",
            "SuperProng (&le;2 mở đầu thưởng) + Không biên + 1 căn cứ",
            "Trung lập <code>53/57</code>, loại vĩnh viễn khi mất căn cứ; 2 căn cứ góc đối phương",
            "Đồng đội 4P + mở đầu thưởng đi thẳng bằng SuperProng (&le;4)",
            "Đồng đội 4P + cuộn vòng Không biên",
            "Đồng đội 4P + SuperProng (&le;4 mở đầu thưởng) + Không biên",
            "4 căn cứ mỗi bên, 0 Pod dự bị, cấm chồng quân, cấm nước đi tổ hợp; 1 căn cứ",
            "Chương 6 &mdash; Ký hiệu nước đi chính thức &amp; đặc tả OCTI-FEN",
            "Cú pháp ký hiệu nước đi",
            "Cú pháp",
            "Hành động",
            "Ví dụ",
            "Đặt Pod dự bị lên ô OCTI",
            "Giải cứu Pod bị bắt của chính bạn lên ô OCTI đang chiếm giữ (chỉ <code>2P_3B*</code> &amp; <code>4P*</code>)",
            "Cắm Prong thường vào lỗ cắm <code>d &isin; {a..h}</code>",
            "Cắm SuperProng vàng vào lỗ cắm <code>d</code>",
            "Bước 1 ô không bắt quân",
            "Nhảy đơn hoặc chuỗi nhảy liên tiếp (<code>x</code> = bắt trọn chồng quân tùy chọn)",
            "Nước đi tổ hợp tách các Pod xuất phát từ cùng một ô",
            "1&ndash;4 pha mở đầu thưởng đi thẳng bằng SuperProng (1 cho mỗi SP trên bàn; nhấc <code>x</code> ngay lập tức) + lượt chính",
            "Định dạng chuỗi thế cờ OCTI-FEN",
            "Trạng thái bàn cờ được tuần tự hóa thành chuỗi <b>OCTI-FEN</b> phân tách bằng dấu hai chấm (11 trường cho <code>2</code> / <code>OFK</code>; 13 trường cho <code>4</code>):",
            "<code>mode</code>: <code>2</code>, <code>4</code> hoặc <code>OFK</code> &bull; <code>bases</code>: <code>3</code> hoặc <code>1</code> &bull; <code>super</code> / <code>edgeless</code>: <code>0</code> hoặc <code>1</code>.",
            "<code>status</code>: <code>N</code> (chưa bắt đầu), <code>P</code> (đang diễn ra), <code>O</code> (kết thúc ván), <code>R</code> (đầu hàng).",
            "<code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code> hoặc <code>-</code> &bull; <code>elimMask</code>: mặt nạ bit loại người chơi 4P (<code>A=1, B=2, C=4, D=8</code>).",
            "<code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (ví dụ: <code>4,0,25,0</code>).",
            "<code>squares</code>: danh sách Pod cách nhau bằng dấu phẩy theo thứ tự hàng-trước <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> (ví dụ: <code>A44ab!,B55e!</code>), hoặc <code>-</code> nếu bàn trống.",
            "<code>noProgressPlies</code> (trường tùy chọn thứ 12/14 ở cuối): số nước đi liên tiếp không tiến triển (<code>0..1000</code>, mặc định là <code>0</code> khi lược bỏ; hàm chuẩn <code>toPositionString()</code> xuất chuỗi 11/13 trường).",
            "Chương 7 &mdash; Hệ thống Elo hai nhóm &amp; luật đấu trường cạnh tranh",
            "Hệ số chính thức bắt đầu từ <b>1200 Elo</b> trên ba bảng xếp hạng biến thể đối kháng 1 đấu 1 độc lập: <b><code>2P_3B</code></b>, <b><code>2P_1B</code></b> và <b><code>OFK</code></b>. Tất cả các ván 4 người chơi (<code>4P*</code>), SuperProng (<code>*_SP</code>), Không biên (<code>*_ED</code>), biến thể kết hợp (<code>*_SPED</code>) và ngoại tuyến đều là ván giao hữu (không tính Elo).",
            "<b>Nhóm 1 &mdash; Bảng xếp hạng dành riêng cho người chơi (Bảng xếp hạng &amp; Danh sách người chơi):</b><br>Chỉ ghi nhận các ván trực tuyến tính Elo Người đấu Người với hệ số tiêu chuẩn <code>K = 32</code>. Hệ số chỉ dành cho người chơi được tách biệt hoàn toàn khỏi mọi ván đấu có robot.",
            "<b>Nhóm 2 &mdash; Đấu trường mở Robot &amp; người chơi (Bảng xếp hạng &amp; Danh bạ Robot):</b><br>Ghi nhận các ván trực tuyến tính Elo có ít nhất một robot AI đã đăng ký tham gia (Người đấu Robot và Robot đấu Robot) với ba cơ chế chống cày điểm:<ul><li><b>Ván đấu Người đấu Robot:</b> Thi đấu tính Elo không giới hạn số ván ở hệ số tiêu chuẩn <code>K = 32</code> (kể cả khi đấu với robot của chính bạn).</li><li><b>Hệ số K Robot đấu Robot:</b> Các ván giữa hai chủ sở hữu khác nhau dùng hệ số chuẩn <code>K = 32</code>; các ván tự kiểm thử cùng chủ sở hữu dùng hệ số giảm <code>K = 16</code>.</li><li><b>Giới hạn cặp đấu Robot đấu Robot &amp; chống đầu hàng sớm:</b> Các ván Robot đấu Robot được giới hạn tối đa <b>4 ván tính Elo trong mỗi khung 24 giờ trượt</b> cho mỗi cặp robot không xét thứ tự (từ ván thứ 5 trở đi tự động chuyển sang giao hữu; không giới hạn số ván mỗi ngày trước các robot đối thủ khác nhau), và mọi ván đầu hàng trước <b>8 nước đi</b> đều ghi nhận biến động 0 Elo.</li></ul>"
        }
    };
    return kPacks;
}

const LocalizedRulebookPack *findLocalizedPack(const std::string &normLang)
{
    const auto &packs = localizedPacks();
    for (const auto &p : packs) {
        if (normLang == p.lang) {
            return &p;
        }
    }
    return nullptr;
}

std::string renderLocalizedDesktopRulebookHtml(const LocalizedRulebookPack &p,
                                               bool lightSurface)
{
    const char *pageBg   = lightSurface ? "#f8f3e8" : "#0b0f17";
    const char *cardBg   = lightSurface ? "#ffffff" : "#121927";
    const char *altBg    = lightSurface ? "#f1eadb" : "#182235";
    const char *figBg    = lightSurface ? "#efe8d8" : "#080c14";
    const char *fgHex    = lightSurface ? "#1c2430" : "#f1f5f9";
    const char *dimHex   = lightSurface ? "#526071" : "#94a3b8";
    const char *headHex  = lightSurface ? "#7c4d12" : "#fde047";
    const char *subHex   = lightSurface ? "#8a5a19" : "#c8a253";
    const char *codeBg   = lightSurface ? "#ede4d3" : "#090d16";
    const char *codeFg   = lightSurface ? "#6b3410" : "#fde68a";
    const char *lineHex  = lightSurface ? "#d4c7b0" : "#28354d";
    const char *thBg     = lightSurface ? "#e6dbc6" : "#1b2740";

    std::ostringstream os;
    os << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
       << "<html lang=\"" << p.lang << "\"><head>"
       << "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"
       << "<title>" << p.docTitle << "</title>"
       << "<style type=\"text/css\">\n"
       << "body { background-color:" << pageBg << "; color:" << fgHex
       << "; font-family:'Segoe UI',Ubuntu,sans-serif; font-size:13.5px; margin:12px; }\n"
       << "h1 { color:" << headHex << "; font-size:21px; margin-top:0px; margin-bottom:4px; }\n"
       << "h2 { color:" << subHex << "; font-size:17px; margin-top:4px; margin-bottom:8px; }\n"
       << "h3 { color:" << fgHex << "; font-size:14.5px; margin-top:12px; margin-bottom:6px; }\n"
       << "p { margin-top:0px; margin-bottom:8px; line-height:1.48; }\n"
       << "li { margin-bottom:5px; line-height:1.45; }\n"
       << "code { background-color:" << codeBg << "; color:" << codeFg
       << "; font-family:Consolas,'DejaVu Sans Mono',monospace; font-size:12.5px; }\n"
       << "</style></head><body>\n";

    /* Banner Card */
    os << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << subHex
       << ";background-color:" << cardBg << ";\">"
       << "<tr><td>"
       << "<h1>" << p.docTitle << "</h1>"
       << "<p style=\"color:" << dimHex << ";margin-bottom:6px;\">"
       << p.bannerSub
       << "</p>"
       << "<p style=\"font-size:12px;color:" << subHex << ";margin-bottom:0px;\">"
       << "<b>" << p.chaptersLabel << "</b> ";
    for (std::size_t i = 0; i < p.sections.size(); ++i) {
        if (i > 0) {
            os << " &bull; ";
        }
        os << (i + 1) << ". " << p.sections[i].title;
    }
    os << "</p></td></tr></table>\n";

    /* Chapter 1 */
    os << "<a name=\"ch1\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>" << p.ch1Heading << "</h2>"
       << "<p>" << p.ch1Intro << "</p>"
       << "<ul>"
       << "<li><b>Pod / Prong / SuperProng:</b> "
       << p.ch1GlossPod << " &bull; "
       << p.ch1GlossProng << " &bull; "
       << p.ch1GlossSuperProng << "</li>"
       << "<li>" << p.ch1GridBullet << "</li>"
       << "<li>" << p.ch1ResBullet << "</li>"
       << "<li>" << p.ch1CompassBullet << "</li>"
       << "</ul>";
    appendI18nFigureTable(os, "rule_01_2p_3b_setup.png", p.fig1Caption, figBg, lineHex, dimHex);
    appendI18nFigureTable(os, "rule_02_2p_1b_setup.png", p.fig2Caption, figBg, lineHex, dimHex);
    appendI18nFigureTable(os, "rule_03_prongs_8dir.png", p.fig3Caption, figBg, lineHex, dimHex);
    os << "</td></tr></table>\n";

    /* Chapter 2 */
    os << "<a name=\"ch2\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>" << p.ch2Heading << "</h2>"
       << "<p>" << p.ch2Intro << "</p>"
       << "<ol>"
       << "<li>" << p.ch2Act1 << "</li>"
       << "<li>" << p.ch2Act2 << "</li>"
       << "<li>" << p.ch2Act3 << "</li>"
       << "<li>" << p.ch2Act4 << "</li>"
       << "</ol>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"9\" style=\"margin-bottom:10px;border-left:4px solid "
       << subHex << ";background-color:" << altBg << ";\"><tr><td>"
       << p.ch2CaptureBox
       << "</td></tr></table>";
    appendI18nFigureTable(os, "rule_04_step_vs_jump.png", p.fig4Caption, figBg, lineHex, dimHex);
    appendI18nFigureTable(os, "rule_05_multihop_chain.png", p.fig5Caption, figBg, lineHex, dimHex);
    appendI18nFigureTable(os, "rule_07_reserve_liberate.png", p.fig7Caption, figBg, lineHex, dimHex);
    os << "</td></tr></table>\n";

    /* Chapter 3 */
    os << "<a name=\"ch3\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>" << p.ch3Heading << "</h2>"
       << "<ul>"
       << "<li>" << p.ch3StackBullet << "</li>"
       << "<li>" << p.ch3CaptureBullet << "</li>"
       << "<li>" << p.ch3LoopBullet << "</li>"
       << "<li>" << p.ch3LibBullet << "</li>"
       << "<li>" << p.ch3EdgeBullet << "</li>"
       << "<li>" << p.ch3SuperBullet << "</li>"
       << "</ul>";
    appendI18nFigureTable(os, "rule_06_stack_split_loop.png", p.fig6Caption, figBg, lineHex, dimHex);
    appendI18nFigureTable(os, "rule_08_edgeless_wrap.png", p.fig8Caption, figBg, lineHex, dimHex);
    appendI18nFigureTable(os, "rule_09_superprongs.png", p.fig9Caption, figBg, lineHex, dimHex);
    os << "</td></tr></table>\n";

    /* Chapter 4 */
    os << "<a name=\"ch4\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>" << p.ch4Heading << "</h2>"
       << "<ul>"
       << "<li>" << p.ch4Win3b << "</li>"
       << "<li>" << p.ch4Win1b << "</li>"
       << "<li>" << p.ch4WinOfk << "</li>"
       << "<li>" << p.ch4Win4p << "</li>"
       << "<li>" << p.ch4Elim4p << "</li>"
       << "<li>" << p.ch4Stalemate << "</li>"
       << "<li>" << p.ch4NoProgress << "</li>"
       << "</ul>";
    appendI18nFigureTable(os, "rule_10_4p_partnership.png", p.fig10Caption, figBg, lineHex, dimHex);
    appendI18nFigureTable(os, "rule_11_ofk_6x7.png", p.fig11Caption, figBg, lineHex, dimHex);
    appendI18nFigureTable(os, "rule_12_victory_infiltration.png", p.fig12Caption, figBg, lineHex, dimHex);
    os << "</td></tr></table>\n";

    /* Chapter 5 */
    os << "<a name=\"ch5\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>" << p.ch5Heading << "</h2>"
       << "<p>" << p.ch5Intro << "</p>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"6\" border=\"1\" "
       << "style=\"border-collapse:collapse;border-color:" << lineHex << ";font-size:12.5px;\">"
       << "<tr bgcolor=\"" << thBg << "\">"
       << "<th align=\"left\">" << p.ch5ThCode << "</th>"
       << "<th align=\"left\">" << p.ch5ThGrid << "</th>"
       << "<th align=\"left\">" << p.ch5ThPlayers << "</th>"
       << "<th align=\"left\">" << p.ch5ThPods << "</th>"
       << "<th align=\"left\">" << p.ch5ThProngs << "</th>"
       << "<th align=\"left\">" << p.ch5ThMod << "</th>"
       << "<th align=\"left\">" << p.ch5ThRated << "</th></tr>"
       << "<tr><td><code>2P_3B</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>" << p.ch5ModStd3b << "</td><td><b>" << p.ch5Yes << "</b></td></tr>"
       << "<tr><td><code>2P_3B_SP</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>" << p.ch5ModSp3b << "</td><td>" << p.ch5Casual << "</td></tr>"
       << "<tr><td><code>2P_3B_ED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>" << p.ch5ModEd3b << "</td><td>" << p.ch5Casual << "</td></tr>"
       << "<tr><td><code>2P_3B_SPED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>" << p.ch5ModSped3b << "</td><td>" << p.ch5Casual << "</td></tr>"
       << "<tr><td><code>2P_1B</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>" << p.ch5ModBlitz1b << "</td><td><b>" << p.ch5Yes << "</b></td></tr>"
       << "<tr><td><code>2P_1B_SP</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>" << p.ch5ModSp1b << "</td><td>" << p.ch5Casual << "</td></tr>"
       << "<tr><td><code>2P_1B_ED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>" << p.ch5ModEd1b << "</td><td>" << p.ch5Casual << "</td></tr>"
       << "<tr><td><code>2P_1B_SPED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>" << p.ch5ModSped1b << "</td><td>" << p.ch5Casual << "</td></tr>"
       << "<tr><td><code>4P</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>12 / 0</td><td>" << p.ch5Mod4p << "</td><td>" << p.ch5Casual << "</td></tr>"
       << "<tr><td><code>4P_SP</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>11 / 1</td><td>" << p.ch5Mod4pSp << "</td><td>" << p.ch5Casual << "</td></tr>"
       << "<tr><td><code>4P_ED</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>12 / 0</td><td>" << p.ch5Mod4pEd << "</td><td>" << p.ch5Casual << "</td></tr>"
       << "<tr><td><code>4P_SPED</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>11 / 1</td><td>" << p.ch5Mod4pSped << "</td><td>" << p.ch5Casual << "</td></tr>"
       << "<tr><td><code>OFK</code></td><td>6&times;7</td><td>2 (1v1)</td><td>4 + 0</td><td>12 / 0</td><td>" << p.ch5ModOfk << "</td><td><b>" << p.ch5Yes << "</b></td></tr>"
       << "</table></td></tr></table>\n";

    /* Chapter 6 */
    os << "<a name=\"ch6\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>" << p.ch6Heading << "</h2>"
       << "<h3>" << p.ch6GrammarSub << "</h3>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"6\" border=\"1\" "
       << "style=\"border-collapse:collapse;border-color:" << lineHex << ";font-size:12.5px;margin-bottom:10px;\">"
       << "<tr bgcolor=\"" << thBg << "\"><th align=\"left\">" << p.ch6ThSyntax
       << "</th><th align=\"left\">" << p.ch6ThAction
       << "</th><th align=\"left\">" << p.ch6ThExample << "</th></tr>"
       << "<tr><td><code>xy</code></td><td>" << p.ch6ActDeploy << "</td><td><code>33</code></td></tr>"
       << "<tr><td><code>xy*</code></td><td>" << p.ch6ActLiberate << "</td><td><code>53*</code>, <code>57*</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;+&lt;d&gt;</code></td><td>" << p.ch6ActAddNormal << "</td><td><code>55acg+b</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;+&lt;d&gt;!</code></td><td>" << p.ch6ActAddSuper << "</td><td><code>33+b!</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;-x'y'</code></td><td>" << p.ch6ActStep << "</td><td><code>44ab-45</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;-x1y1[x]-...</code></td><td>" << p.ch6ActJump << "</td><td><code>33abd-55-57x-75x</code></td></tr>"
       << "<tr><td><code>op1,op2[,...]</code></td><td>" << p.ch6ActComposite << "</td><td><code>44a-45,44beg-66x-64x-44</code></td></tr>"
       << "<tr><td><code>pre1;[pre2;...]reg</code></td><td>" << p.ch6ActPrelude << "</td><td><code>44ab!-66x;66ab!-67</code></td></tr>"
       << "</table>"
       << "<h3>" << p.ch6FenSub << "</h3>"
       << "<p>" << p.ch6FenIntro << "</p>"
       << "<p><code>mode:bases:super:edgeless:status:turn:winner:elimMask:resA:resB[:resC:resD]:squares[:noProgressPlies]</code></p>"
       << "<ul>"
       << "<li>" << p.ch6FenB1 << "</li>"
       << "<li>" << p.ch6FenB2 << "</li>"
       << "<li>" << p.ch6FenB3 << "</li>"
       << "<li>" << p.ch6FenB4 << "</li>"
       << "<li>" << p.ch6FenB5 << "</li>"
       << "<li>" << p.ch6FenB6 << "</li>"
       << "</ul>"
       << "</td></tr></table>\n";

    /* Chapter 7 */
    os << "<a name=\"ch7\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:8px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>" << p.ch7Heading << "</h2>"
       << "<p>" << p.ch7Intro << "</p>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"9\" style=\"margin-bottom:8px;border-left:4px solid "
       << subHex << ";background-color:" << altBg << ";\"><tr><td>"
       << p.ch7Pool1Box
       << "</td></tr></table>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"9\" style=\"border-left:4px solid "
       << headHex << ";background-color:" << altBg << ";\"><tr><td>"
       << p.ch7Pool2Box
       << "</td></tr></table>"
       << "</td></tr></table>\n";

    os << "</body></html>\n";
    return os.str();
}

} // namespace

const std::vector<std::string> &rulebookSupportedLanguages()
{
    static const std::vector<std::string> kLanguages = {
        "en", "zh-TW", "zh-CN", "fr", "ro", "ru", "es", "de",
        "ja", "tr", "pt-BR", "ko", "pl", "it", "id", "vi"
    };
    return kLanguages;
}

std::string normalizeRulebookLanguage(const std::string &lang)
{
    std::size_t start = 0;
    while (start < lang.size() &&
           std::isspace(static_cast<unsigned char>(lang[start]))) {
        ++start;
    }
    std::size_t end = lang.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(lang[end - 1]))) {
        --end;
    }
    if (start >= end || (end - start) > 32u) {
        return "en";
    }

    std::string norm;
    norm.reserve(end - start);
    for (std::size_t i = start; i < end; ++i) {
        unsigned char uch = static_cast<unsigned char>(lang[i]);
        if (uch < 0x20u || uch >= 0x7Fu) {
            return "en";
        }
        char c = lang[i];
        if (c == '_') {
            c = '-';
        } else if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
        norm.push_back(c);
    }

    if (norm == "en") return "en";
    if (norm == "zh-tw") return "zh-TW";
    if (norm == "zh-cn") return "zh-CN";
    if (norm == "fr") return "fr";
    if (norm == "ro") return "ro";
    if (norm == "ru") return "ru";
    if (norm == "es") return "es";
    if (norm == "de") return "de";
    if (norm == "ja") return "ja";
    if (norm == "tr") return "tr";
    if (norm == "pt" ||
        (norm.size() == 5 && norm.compare(0, 3, "pt-") == 0 &&
         norm[3] >= 'a' && norm[3] <= 'z' &&
         norm[4] >= 'a' && norm[4] <= 'z')) {
        return "pt-BR";
    }
    if (norm == "ko") return "ko";
    if (norm == "pl") return "pl";
    if (norm == "it") return "it";
    if (norm == "id" || norm == "in") return "id";
    if (norm == "vi") return "vi";
    return "en";
}

const std::vector<RulebookSection> &rulebookSections(const std::string &lang)
{
    const std::string norm = normalizeRulebookLanguage(lang);
    if (norm == "en") {
        return rulebookSections();
    }
    const LocalizedRulebookPack *pack = findLocalizedPack(norm);
    if (pack != nullptr) {
        return pack->sections;
    }
    return rulebookSections();
}

std::string buildDesktopRulebookHtml(bool lightSurface, const std::string &lang)
{
    const std::string norm = normalizeRulebookLanguage(lang);
    if (norm == "en") {
        return buildDesktopRulebookHtml(lightSurface);
    }
    const LocalizedRulebookPack *pack = findLocalizedPack(norm);
    if (pack != nullptr) {
        return renderLocalizedDesktopRulebookHtml(*pack, lightSurface);
    }
    return buildDesktopRulebookHtml(lightSurface);
}

} // namespace octi

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

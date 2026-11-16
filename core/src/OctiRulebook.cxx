/*
 * OctiRulebook.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiRulebook.hxx"
#include "OctiRulebookAssets.hxx"
#include <sstream>

namespace octi {

namespace {

struct RulebookImageDescriptor {
    const char *fileName;
    const std::uint8_t *pngData;
    std::size_t pngSize;
};

const RulebookImageDescriptor kImageDescriptors[OCTI_RULEBOOK_IMAGE_COUNT] = {
    {
        "rule_01_2p_3b_setup.png",
        kOctiRulebookPng_rule_01_2p_3b_setup,
        kOctiRulebookPngSize_rule_01_2p_3b_setup
    },
    {
        "rule_02_2p_1b_setup.png",
        kOctiRulebookPng_rule_02_2p_1b_setup,
        kOctiRulebookPngSize_rule_02_2p_1b_setup
    },
    {
        "rule_03_prongs_8dir.png",
        kOctiRulebookPng_rule_03_prongs_8dir,
        kOctiRulebookPngSize_rule_03_prongs_8dir
    },
    {
        "rule_04_step_vs_jump.png",
        kOctiRulebookPng_rule_04_step_vs_jump,
        kOctiRulebookPngSize_rule_04_step_vs_jump
    },
    {
        "rule_05_multihop_chain.png",
        kOctiRulebookPng_rule_05_multihop_chain,
        kOctiRulebookPngSize_rule_05_multihop_chain
    },
    {
        "rule_06_stack_split_loop.png",
        kOctiRulebookPng_rule_06_stack_split_loop,
        kOctiRulebookPngSize_rule_06_stack_split_loop
    },
    {
        "rule_07_reserve_liberate.png",
        kOctiRulebookPng_rule_07_reserve_liberate,
        kOctiRulebookPngSize_rule_07_reserve_liberate
    },
    {
        "rule_08_edgeless_wrap.png",
        kOctiRulebookPng_rule_08_edgeless_wrap,
        kOctiRulebookPngSize_rule_08_edgeless_wrap
    },
    {
        "rule_09_superprongs.png",
        kOctiRulebookPng_rule_09_superprongs,
        kOctiRulebookPngSize_rule_09_superprongs
    },
    {
        "rule_10_4p_partnership.png",
        kOctiRulebookPng_rule_10_4p_partnership,
        kOctiRulebookPngSize_rule_10_4p_partnership
    },
    {
        "rule_11_ofk_6x7.png",
        kOctiRulebookPng_rule_11_ofk_6x7,
        kOctiRulebookPngSize_rule_11_ofk_6x7
    },
    {
        "rule_12_victory_infiltration.png",
        kOctiRulebookPng_rule_12_victory_infiltration,
        kOctiRulebookPngSize_rule_12_victory_infiltration
    },
};

bool isSafeAssetFileName(const std::string &fileName)
{
    if (fileName.empty() || fileName.size() > 64u ||
        fileName.find('\0') != std::string::npos ||
        fileName.find('/') != std::string::npos ||
        fileName.find('\\') != std::string::npos ||
        fileName.find("..") != std::string::npos) {
        return false;
    }
    for (char ch : fileName) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (uch < 0x20u || uch >= 0x7Fu) {
            return false;
        }
    }
    return true;
}

void appendFigureTable(std::ostringstream &os,
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

} // namespace

std::vector<std::string> rulebookImageFileNames()
{
    std::vector<std::string> names;
    names.reserve(OCTI_RULEBOOK_IMAGE_COUNT);
    for (std::size_t i = 0; i < OCTI_RULEBOOK_IMAGE_COUNT; ++i) {
        names.emplace_back(kImageDescriptors[i].fileName);
    }
    return names;
}

const std::uint8_t *rulebookImageBytes(const std::string &fileName,
                                       std::size_t &outSize)
{
    outSize = 0u;
    if (!isSafeAssetFileName(fileName)) {
        return nullptr;
    }
    for (std::size_t i = 0; i < OCTI_RULEBOOK_IMAGE_COUNT; ++i) {
        if (fileName == kImageDescriptors[i].fileName) {
            outSize = kImageDescriptors[i].pngSize;
            return kImageDescriptors[i].pngSize > 0u
                ? kImageDescriptors[i].pngData
                : nullptr;
        }
    }
    return nullptr;
}

const std::vector<RulebookSection> &rulebookSections()
{
    static const std::vector<RulebookSection> kSections = {
        {
            "ch1",
            "Chapter 1",
            "Board Geometry, Pods, Prongs & Setup",
            "9x9 coordinates, starting reserves, and 8 compass sockets a-h"
        },
        {
            "ch2",
            "Chapter 2",
            "Complete Turn Actions",
            "Prong insert/reposition, 1-step, multi-hop jumps, reserve & liberate"
        },
        {
            "ch3",
            "Chapter 3",
            "Stacking, Composite & Appendix A",
            "Multi-pod comma moves, A.1 loop-backs, Edgeless wrap & SuperProngs"
        },
        {
            "ch4",
            "Chapter 4",
            "Victory, Elimination & Stalemate",
            "3-base vs 1-base infiltration, 4P elimination/revival & OFK 6x7"
        },
        {
            "ch5",
            "Chapter 5",
            "Canonical 13-Variant Matrix",
            "Board dimensions, pod/prong budgets, modifiers & rated eligibility"
        },
        {
            "ch6",
            "Chapter 6",
            "Move Notation & OCTI-FEN Grammar",
            "Official algebraic move grammar and 11/13-token OCTI-FEN format"
        },
        {
            "ch7",
            "Chapter 7",
            "Dual-Pool Elo & Arena Rules",
            "Human-Only Pool 1 vs Open Bot & Human Arena Pool 2 safeguards"
        },
    };
    return kSections;
}

std::string buildDesktopRulebookHtml(bool lightSurface)
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
       << "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"
       << "<title>OCTI Official Illustrated Rulebook</title>"
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
       << "<h1>OCTI Official Illustrated Rulebook</h1>"
       << "<p style=\"color:" << dimHex << ";margin-bottom:0px;\">"
       << "Complete multi-variant rules, tactical mechanics, Appendix A edge cases, "
       << "official move/FEN grammar, and Dual-Pool Elo competitive regulations."
       << "</p></td></tr></table>\n";

    /* Chapter 1 */
    os << "<a name=\"ch1\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>Chapter 1 &mdash; Board Geometry, Pods, Prongs &amp; Starting Setup</h2>"
       << "<p>OCTI is a deterministic, perfect-information abstract strategy game of vector construction. "
       << "Every pod enters the board with zero movement prongs and earns directional mobility as players "
       << "insert prongs one turn at a time.</p>"
       << "<ul>"
       << "<li><b>9&times;9 Standard Grid:</b> Columns <code>x = 1..9</code> run West-to-East (left to right); "
       << "rows <code>y = 1..9</code> run South-to-North (bottom to top). Every square is written as two digits "
       << "<code>xy</code> (for example, <code>53</code> is column 5, row 3).</li>"
       << "<li><b>2-Player Starting Resources (<code>2P_3B</code> &amp; <code>2P_1B</code>):</b> Player A starts with "
       << "<b>3 pods</b> on home OCTI bases <code>33</code>, <code>53</code>, and <code>73</code>; Player B starts with "
       << "<b>3 pods</b> on home OCTI bases <code>37</code>, <code>57</code>, and <code>77</code>. Each player also holds "
       << "<b>4 reserve pods</b> (7 pods total per side) and <b>25 prongs</b> (or <b>24 normal prongs + 1 gold SuperProng</b> "
       << "in <code>_SP</code> variants).</li>"
       << "<li><b>8 Compass Prong Sockets (<code>a</code>&ndash;<code>h</code>):</b> Every octagonal pod has 8 perimeter sockets "
       << "mapped clockwise from North: <code>a</code>=N (<code>0,+1</code>), <code>b</code>=NE (<code>+1,+1</code>), "
       << "<code>c</code>=E (<code>+1,0</code>), <code>d</code>=SE (<code>+1,-1</code>), <code>e</code>=S (<code>0,-1</code>), "
       << "<code>f</code>=SW (<code>-1,-1</code>), <code>g</code>=W (<code>-1,0</code>), and <code>h</code>=NW (<code>-1,+1</code>).</li>"
       << "</ul>";
    appendFigureTable(os, "rule_01_2p_3b_setup.png",
                      "Figure 1: Standard 9x9 2-Player setup (2P_3B) with home rows 33/53/73 vs. 37/57/77.",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_02_2p_1b_setup.png",
                      "Figure 2: 2P_1B (1-Base Blitz) uses the identical setup, but infiltrating any 1 enemy base (55ab-57x) wins immediately.",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_03_prongs_8dir.png",
                      "Figure 3: The 8 compass sockets a-h around pod A55acg, supply insertion (55acg+b), and reposition (33b+a-b).",
                      figBg, lineHex, dimHex);
    os << "</td></tr></table>\n";

    /* Chapter 2 */
    os << "<a name=\"ch2\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>Chapter 2 &mdash; Complete Turn Actions</h2>"
       << "<p>Player A moves first and turns alternate strictly (in <code>4P</code>, turn order rotates "
       << "<code>A &rarr; B &rarr; C &rarr; D</code>, skipping eliminated seats). Passing is forbidden; on your turn "
       << "you must execute <b>exactly one</b> regular turn action:</p>"
       << "<ol>"
       << "<li><b>Insert a Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Take 1 prong from your supply and insert it into an "
       << "empty socket on one of your pods on the board (e.g., <code>55acg+b</code>; or <code>33+b!</code> for a gold SuperProng).</li>"
       << "<li><b>Reposition a Normal Prong (<code>xy&lt;cfg&gt;+&lt;add&gt;-&lt;rm&gt;</code>):</b> Move an existing normal prong on "
       << "one of your pods to another empty socket on that <i>same</i> pod (e.g., <code>33b+a-b</code>). Costs 0 supply prongs. "
       << "Forbidden on SuperProngs (<code>!</code>) and forbidden in OCTI for Kids (<code>OFK</code>).</li>"
       << "<li><b>1-Square Step (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Move a pod 1 square along one of its installed prong directions "
       << "onto an empty square or a friendly/allied stack (e.g., <code>44ab-45</code>). Steps never capture and cannot enter an enemy square.</li>"
       << "<li><b>2-Square Jump &amp; Multi-Hop Chain (<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> Jump 2 squares along an "
       << "installed prong direction over an adjacent occupied square (friendly or enemy) onto an empty square beyond it. You may chain "
       << "additional jumps with that same pod along any of its installed prongs and stop early after any legal hop.</li>"
       << "<li><b>Deploy Reserve Pod (<code>xy</code>) or Liberate Captured Pod (<code>xy*</code>):</b> Place 1 reserve pod onto a "
       << "non-enemy-occupied own home base (or an enemy base you already occupy), or liberate 1 captured pod (<code>xy*</code>) onto an "
       << "OCTI square you already occupy when eligible.</li>"
       << "</ol>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"9\" style=\"margin-bottom:10px;border-left:4px solid "
       << subHex << ";background-color:" << altBg << ";\"><tr><td>"
       << "<b>Optional Capture &amp; Prong Plunder:</b> Jumping over an enemy square makes capture <b>optional</b> "
       << "(append <code>x</code> to capture, e.g., <code>44ab-66x</code>, or omit <code>x</code> to leave the jumped enemy intact). "
       << "Captured enemy pods are removed at the end of the turn into their owner's <code>capturedPods</code> pool, while "
       << "<b>all prongs attached to them transfer immediately into the capturing player's free prong supply</b>."
       << "</td></tr></table>";
    appendFigureTable(os, "rule_04_step_vs_jump.png",
                      "Figure 4: Pod A44ab stepping 1 square North (44ab-45) vs. jumping 2 squares NE over B55e with optional capture (44ab-66x).",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_05_multihop_chain.png",
                      "Figure 5: Multi-hop chain 33abd-55-57x-75x vaulting friendly A44h, pivoting North to capture B56e, then SE to capture B66c.",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_07_reserve_liberate.png",
                      "Figure 7: Deploying a free reserve pod onto empty base 33 vs. liberating a captured pod (53* or 57*) while A57ab holds an enemy base.",
                      figBg, lineHex, dimHex);
    os << "</td></tr></table>\n";

    /* Chapter 3 */
    os << "<a name=\"ch3\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>Chapter 3 &mdash; Stacking, Composite Moves &amp; Appendix A Edge Cases</h2>"
       << "<ul>"
       << "<li><b>Friendly Stacking &amp; Composite Multi-Pod Moves (<code>,</code>):</b> Multiple friendly pods (and diagonal-partner pods "
       << "in <code>4P</code>) may stack on the same square (forbidden in <code>OFK</code>). Any subset of your own pods that <b>begin the turn "
       << "on the same square</b> may move together or split along separate step/jump paths in one turn, joined by commas "
       << "(e.g., <code>44a-45,44beg-34</code>). Pod placement and prong edits never combine with movement.</li>"
       << "<li><b>End-of-Turn Capture Removal:</b> Enemy stacks jumped with <code>x</code> stay on the board until the end of the turn "
       << "(so composite stack-mates may also jump them), then are removed together. However, a single pod may <b>never jump over the exact "
       << "same square twice</b> during one hop chain.</li>"
       << "<li><b>Appendix A.1 &mdash; Starting-Square Vacancy &amp; Loop-Back Jumps:</b> During a single or composite move from square "
       << "<code>S</code>, all pods initially on <code>S</code> (including stationary stack-mates) are treated as vacated for jump geometry: "
       << "a jumping pod from <code>S</code> <b>cannot jump over <code>S</code></b>, yet <b>can legally land back onto <code>S</code></b> at the "
       << "end of a multi-hop loop (e.g., <code>44beg-66x-64x-44</code> rejoining stationary <code>A44a</code> on <code>44</code>).</li>"
       << "<li><b>Liberation Clearance Exact Conditions:</b> Liberating a captured pod (<code>xy*</code>) always requires the target OCTI square "
       << "to be <b>already occupied by at least one of your own pods</b>. In <code>2P</code>, liberating onto your own home base additionally "
       << "requires occupying at least one enemy base; liberating directly onto an enemy base you already occupy requires only "
       << "<code>capturedPods &gt; 0</code>. In <code>4P</code>, liberating onto your own or active partner's base requires personally occupying "
       << "a neutral (<code>53</code>/<code>57</code>) or enemy OCTI square.</li>"
       << "<li><b>Edgeless Torus Wrapping (<code>_ED</code>):</b> Column <code>1</code> wraps to column <code>9</code> and row <code>1</code> wraps "
       << "to row <code>9</code> (including diagonals) for both steps (<code>15g-95</code>) and 2-square jumps (<code>97b-29x</code> jumping <code>B18e</code>).</li>"
       << "<li><b>SuperProngs &amp; Bonus Preludes (<code>_SP</code>, <code>;</code>):</b> A gold SuperProng (<code>!</code>) is permanent once inserted. "
       << "Before your regular turn action, you may execute <b>1 or 2 bonus prelude moves</b> delimited by semicolons "
       << "(e.g., <code>44ab!-66x;66ab!-67</code>). Each prelude moves along a single SuperProng socket, and its captures resolve <i>immediately</i> "
       << "before the next segment. The same SuperProng socket on the same pod cannot be fired twice in two preludes on the same turn.</li>"
       << "</ul>";
    appendFigureTable(os, "rule_06_stack_split_loop.png",
                      "Figure 6: Stack [A44a, A44beg] splitting via 44a-45,44beg-34 or looping back onto stationary A44a via 44beg-66x-64x-44 (Appendix A.1).",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_08_edgeless_wrap.png",
                      "Figure 8: Edgeless (_ED) wrap-around step 15g-95 and diagonal wrap jump capture 97b-29x across the portal border.",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_09_superprongs.png",
                      "Figure 9: Gold SuperProng bonus prelude 44ab!-66x; capturing B55e! immediately before regular step 66ab!-67.",
                      figBg, lineHex, dimHex);
    os << "</td></tr></table>\n";

    /* Chapter 4 */
    os << "<a name=\"ch4\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>Chapter 4 &mdash; Victory, Elimination &amp; Stalemate Conditions</h2>"
       << "<ul>"
       << "<li><b><code>2P_3B</code> Family (3-Base Standard):</b> Win immediately at the end of your turn if your pods simultaneously occupy "
       << "<b>all 3</b> enemy home bases (<code>37</code>, <code>57</code>, <code>77</code> for A; <code>33</code>, <code>53</code>, <code>73</code> for B).</li>"
       << "<li><b><code>2P_1B</code> Family (1-Base Blitz):</b> Win immediately when your pod occupies <b>any 1</b> of the 3 enemy home bases.</li>"
       << "<li><b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Win immediately when your pod occupies <b>any 1</b> of the 4 enemy home bases "
       << "(<code>26, 36, 46, 56</code> for A; <code>22, 32, 42, 52</code> for B).</li>"
       << "<li><b><code>4P</code> Partnership Family (Team A&amp;C vs. Team B&amp;D):</b> Diagonal partners win together when allied pods "
       << "simultaneously occupy <b>both enemy corner bases</b> (<code>37</code> &amp; <code>73</code> for A&amp;C; <code>33</code> &amp; <code>77</code> for B&amp;D).</li>"
       << "<li><b>4-Player Base-Capture Elimination &amp; Teammate Revival:</b> In <code>4P</code>, if an enemy pod ends its turn on your corner home "
       << "base (or you have zero board pods and zero legal placements), you are <b>eliminated</b>: your remaining board pods move to "
       << "<code>capturedPods</code> and their prongs transfer to the capturing side. Your surviving partner can <b>revive</b> you by occupying "
       << "your corner home base (once clear of enemies) and playing <code>xy*</code> (e.g., <code>77*</code>).</li>"
       << "<li><b>Total Wipeout / Stalemate:</b> If the next player (or both enemy seats in <code>4P</code>) has <b>zero legal moves</b>, the "
       << "player/team who just moved wins immediately. Timed matches also conclude on clock flag forfeit or resignation.</li>"
       << "</ul>";
    appendFigureTable(os, "rule_10_4p_partnership.png",
                      "Figure 10: 4-Player Partnership (4P): diagonal allies A(33)&C(77) vs. B(37)&D(73), allied stack [A55ab, C55cf], and neutral 53/57.",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_11_ofk_6x7.png",
                      "Figure 11: OCTI for Kids (OFK) on the 6x7 grid: 4 bases per side, 0 reserve pods, permanent prongs, no stacking, 1-base victory (34a-36x).",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_12_victory_infiltration.png",
                      "Figure 12: 2P_3B victory state (status = O) with Player A simultaneously occupying all 3 enemy bases (A37c, A57a, A77g).",
                      figBg, lineHex, dimHex);
    os << "</td></tr></table>\n";

    /* Chapter 5 */
    os << "<a name=\"ch5\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>Chapter 5 &mdash; Canonical 13-Variant Reference Matrix</h2>"
       << "<p>Every match across Web, Desktop, Android, Archive, and Bot SDK uses one of <b>13 canonical variant codes</b>:</p>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"6\" border=\"1\" "
       << "style=\"border-collapse:collapse;border-color:" << lineHex << ";font-size:12.5px;\">"
       << "<tr bgcolor=\"" << thBg << "\">"
       << "<th align=\"left\">Code</th><th align=\"left\">Grid</th><th align=\"left\">Players</th>"
       << "<th align=\"left\">Pods (Start+Res)</th><th align=\"left\">Prongs (N/SP)</th>"
       << "<th align=\"left\">Modifiers &amp; Win Target</th><th align=\"left\">Rated</th></tr>"
       << "<tr><td><code>2P_3B</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>Standard rules; all 3 enemy bases</td><td><b>Yes</b></td></tr>"
       << "<tr><td><code>2P_3B_SP</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>SuperProng <code>!</code> + <code>;</code> preludes; 3 bases</td><td>Casual</td></tr>"
       << "<tr><td><code>2P_3B_ED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>Edgeless torus wrap-around; 3 bases</td><td>Casual</td></tr>"
       << "<tr><td><code>2P_3B_SPED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>SuperProng + Edgeless; 3 bases</td><td>Casual</td></tr>"
       << "<tr><td><code>2P_1B</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>Sudden-death blitz; any 1 enemy base</td><td><b>Yes</b></td></tr>"
       << "<tr><td><code>2P_1B_SP</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>SuperProng + 1-base sudden death</td><td>Casual</td></tr>"
       << "<tr><td><code>2P_1B_ED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>Edgeless + 1-base sudden death</td><td>Casual</td></tr>"
       << "<tr><td><code>2P_1B_SPED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>SuperProng + Edgeless + 1-base</td><td>Casual</td></tr>"
       << "<tr><td><code>4P</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>12 / 0</td><td>Neutral <code>53/57</code>, elimination &amp; revival; 2 bases</td><td>Casual</td></tr>"
       << "<tr><td><code>4P_SP</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>11 / 1</td><td>4P Partnership + SuperProng preludes</td><td>Casual</td></tr>"
       << "<tr><td><code>4P_ED</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>12 / 0</td><td>4P Partnership + Edgeless wrap-around</td><td>Casual</td></tr>"
       << "<tr><td><code>4P_SPED</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>11 / 1</td><td>4P Partnership + SuperProng + Edgeless</td><td>Casual</td></tr>"
       << "<tr><td><code>OFK</code></td><td>6&times;7</td><td>2 (1v1)</td><td>4 + 0</td><td>12 / 0</td><td>4 bases/side, no stack/reposition/composite; 1 base</td><td><b>Yes</b></td></tr>"
       << "</table></td></tr></table>\n";

    /* Chapter 6 */
    os << "<a name=\"ch6\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>Chapter 6 &mdash; Official Move Notation &amp; OCTI-FEN Specification</h2>"
       << "<h3>Move Notation Grammar</h3>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"6\" border=\"1\" "
       << "style=\"border-collapse:collapse;border-color:" << lineHex << ";font-size:12.5px;margin-bottom:10px;\">"
       << "<tr bgcolor=\"" << thBg << "\"><th align=\"left\">Syntax</th><th align=\"left\">Action</th><th align=\"left\">Example</th></tr>"
       << "<tr><td><code>xy</code></td><td>Deploy free reserve pod onto OCTI square</td><td><code>33</code></td></tr>"
       << "<tr><td><code>xy*</code></td><td>Liberate captured pod (or revive eliminated 4P partner)</td><td><code>53*</code>, <code>57*</code>, <code>77*</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;+&lt;d&gt;</code></td><td>Insert normal prong in socket <code>d &isin; {a..h}</code></td><td><code>55acg+b</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;+&lt;d&gt;!</code></td><td>Insert gold SuperProng in socket <code>d</code></td><td><code>33+b!</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;+&lt;add&gt;-&lt;rm&gt;</code></td><td>Reposition normal prong on same pod</td><td><code>33b+a-b</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;-x'y'</code></td><td>1-square non-capturing step</td><td><code>44ab-45</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;-x1y1[x]-...</code></td><td>Single or multi-hop jump chain (<code>x</code> = optional capture)</td><td><code>33abd-55-57x-75x</code></td></tr>"
       << "<tr><td><code>op1,op2[,...]</code></td><td>Composite move splitting pods from the same origin square</td><td><code>44a-45,44beg-34</code></td></tr>"
       << "<tr><td><code>pre1;[pre2;]reg</code></td><td>1&ndash;2 SuperProng bonus preludes followed by regular turn</td><td><code>44ab!-66x;66ab!-67</code></td></tr>"
       << "</table>"
       << "<h3>OCTI-FEN Position String Format</h3>"
       << "<p>Board states serialize into colon-separated <b>OCTI-FEN</b> strings (11 tokens for <code>2</code> / <code>OFK</code>; "
       << "13 tokens for <code>4</code>):</p>"
       << "<p><code>mode:bases:super:edgeless:status:turn:winner:elimMask:resA:resB[:resC:resD]:squares</code></p>"
       << "<ul>"
       << "<li><code>mode</code>: <code>2</code>, <code>4</code>, or <code>OFK</code> &bull; <code>bases</code>: <code>3</code> or <code>1</code> "
       << "&bull; <code>super</code> / <code>edgeless</code>: <code>0</code> or <code>1</code>.</li>"
       << "<li><code>status</code>: <code>N</code> (not started), <code>P</code> (in progress), <code>O</code> (game over), <code>R</code> (resigned).</li>"
       << "<li><code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code>, or <code>-</code> "
       << "&bull; <code>elimMask</code>: 4P elimination bitmask (<code>A=1, B=2, C=4, D=8</code>).</li>"
       << "<li><code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (e.g., <code>4,0,25,0</code>).</li>"
       << "<li><code>squares</code>: comma-separated pods in row-major order <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> "
       << "(e.g., <code>A44ab!,B55e!</code>), or <code>-</code> if empty.</li>"
       << "</ul></td></tr></table>\n";

    /* Chapter 7 */
    os << "<a name=\"ch7\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:8px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>Chapter 7 &mdash; Dual-Pool Elo &amp; Competitive Arena Rules</h2>"
       << "<p>Official ratings begin at <b>1200 Elo</b> on three independent 1v1 variant ladders: "
       << "<b><code>2P_3B</code></b>, <b><code>2P_1B</code></b>, and <b><code>OFK</code></b>. All 4-Player (<code>4P*</code>), "
       << "SuperProng (<code>*_SP</code>), Edgeless (<code>*_ED</code>), combined (<code>*_SPED</code>), and offline matches are casual (unrated).</p>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"9\" style=\"margin-bottom:8px;border-left:4px solid "
       << subHex << ";background-color:" << altBg << ";\"><tr><td>"
       << "<b>Pool 1 &mdash; Human-Only Ladders (Leaderboard &amp; Players Roster):</b><br>"
       << "Tracks exclusively online rated Human-vs-Human matches using standard Elo factor <code>K = 32</code>. "
       << "Human-Only ratings are completely insulated from bot matches."
       << "</td></tr></table>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"9\" style=\"border-left:4px solid "
       << headHex << ";background-color:" << altBg << ";\"><tr><td>"
       << "<b>Pool 2 &mdash; Open Bot &amp; Human Arena (Leaderboard &amp; Robots Directory):</b><br>"
       << "Tracks rated online matches involving at least one registered AI robot (Human-vs-Robot and Robot-vs-Robot) "
       << "with three anti-farming safeguards:"
       << "<ul>"
       << "<li><b>Human-vs-Robot Matches:</b> Unlimited rated play at standard <code>K = 32</code> (including against your own robots).</li>"
       << "<li><b>Bot-vs-Bot K-Factors:</b> Cross-owner matches use standard <code>K = 32</code>; same-owner self-testing uses reduced <code>K = 16</code>.</li>"
       << "<li><b>Bot-vs-Bot Pair Cap &amp; Early-Resign Guard:</b> Bot-vs-Bot matches are capped at <b>4 rated games per rolling 24-hour window</b> per unordered bot pair "
       << "(5th+ auto-downgrades to casual; no daily cap across distinct opponent bots), and resignations before <b>8 plies</b> record 0 Elo delta.</li>"
       << "</ul></td></tr></table>"
       << "</td></tr></table>\n";

    os << "</body></html>\n";
    return os.str();
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

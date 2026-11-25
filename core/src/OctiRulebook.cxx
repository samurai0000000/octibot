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

/*
 * Default English rulebook sections and desktop HTML generator.
 * Language-parameterized overloads rulebookSections(lang) and
 * buildDesktopRulebookHtml(lightSurface, lang) across all 16 LTR locales
 * are implemented in OctiRulebookI18n.cxx and fall back to these functions
 * when lang normalizes to "en".
 */
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
            "Prong insertion, 1-step, multi-hop jumps, reserve & liberate"
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
            "Victory & 4P Elimination",
            "3-base vs 1-base infiltration, 4P elimination & 30/60-ply no-progress draw"
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
            "Human-Only Pool 1 vs Open Robot & Human Arena Pool 2 safeguards"
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
       << "<p style=\"color:" << dimHex << ";margin-bottom:6px;\">"
       << "Complete multi-variant rules, tactical mechanics, Appendix A edge cases, "
       << "official move/FEN grammar, and Dual-Pool Elo competitive regulations."
       << "</p>"
       << "<p style=\"font-size:12px;color:" << subHex << ";margin-bottom:0px;\">"
       << "<b>Chapters:</b> "
       << "1. Setup, Pods &amp; Prongs &bull; "
       << "2. Complete Turn Actions &bull; "
       << "3. Stacking &amp; Appendix A &bull; "
       << "4. Victory &amp; 4P Elimination &bull; "
       << "5. 13-Variant Matrix &bull; "
       << "6. Notation &amp; OCTI-FEN &bull; "
       << "7. Dual-Pool Elo Rules"
       << "</p></td></tr></table>\n";

    /* Chapter 1 */
    os << "<a name=\"ch1\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>Chapter 1 &mdash; Board Geometry, Pods, Prongs &amp; Starting Setup</h2>"
       << "<p>OCTI is a deterministic, perfect-information abstract strategy game of vector construction. "
       << "Every pod enters the board blank (with zero movement directions) and earns directional mobility as players "
       << "insert <b>prongs</b> one turn at a time.</p>"
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
                      "Figure 2: 2P_1B (1-Base Blitz) uses the identical setup, but infiltrating any 1 enemy base (55ab-57x) is enough to win.",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_03_prongs_8dir.png",
                      "Figure 3: The 8 compass sockets a-h around pod A55acg and supply prong insertion (55acg+b) into an empty perimeter socket.",
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
       << "<li><b>Insert a Prong (<code>xy&lt;cfg&gt;+&lt;dir&gt;</code>):</b> Take 1 prong from your free supply and insert it into an "
       << "empty socket on one of your pods on the board (e.g., <code>55acg+b</code>; or <code>33+b!</code> for a gold SuperProng). "
       << "Once inserted, prongs are permanent on that pod&mdash;repositioning an installed prong to another socket is never allowed in any variant.</li>"
       << "<li><b>1-Square Step (<code>xy&lt;cfg&gt;-x'y'</code>):</b> Move a pod 1 square along one of its installed prong directions "
       << "onto an empty square or a stack of your own pods (e.g., <code>44ab-45</code>). A 1-square step <b>never captures</b> and cannot enter a square occupied by another player's pod (including your diagonal partner in <code>4P</code>).</li>"
       << "<li><b>2-Square Jump &amp; Multi-Hop Chain (<code>xy&lt;cfg&gt;-x1y1[x]-x2y2[x]...</code>):</b> Jump 2 squares along an "
       << "installed prong direction over an adjacent occupied square (friendly or enemy) onto an empty square beyond it. You may chain "
       << "additional jumps with that same pod along any of its installed prongs (pivoting mid-chain) and stop early after any legal hop.</li>"
       << "<li><b>Deploy Reserve Pod (<code>xy</code>) or Liberate Captured Pod (<code>xy*</code>):</b> Place 1 reserve pod onto your "
       << "own home base when empty or occupied only by your own pods (or onto an enemy base you already occupy), or&mdash;in 3-Base (<code>2P_3B*</code>) and "
       << "4-Player (<code>4P*</code>) games&mdash;liberate 1 of your own captured pods (<code>xy*</code>) onto an OCTI square you already "
       << "occupy when eligible (liberation does not apply to 1-Base sudden-death variants or <code>OFK</code>).</li>"
       << "</ol>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"9\" style=\"margin-bottom:10px;border-left:4px solid "
       << subHex << ";background-color:" << altBg << ";\"><tr><td>"
       << "<b>Optional Whole-Stack Capture &amp; Prong Plunder:</b> Jumping over an occupied square makes capture <b>optional</b>: "
       << "append <code>x</code> (e.g., <code>44ab-66x</code>) to capture <b>all pods in the jumped stack</b> (whole-stack capture&mdash;"
       << "selective or partial stack capture is never allowed), or omit <code>x</code> (e.g., <code>44ab-66</code>) to leave the jumped stack intact. "
       << "During regular single or composite moves, captured stacks are removed at the end of the turn into each pod owner's <code>capturedPods</code> pool, while "
       << "<b>all prongs attached to them transfer into the capturing player's free prong supply</b>."
       << "</td></tr></table>";
    appendFigureTable(os, "rule_04_step_vs_jump.png",
                      "Figure 4: Pod A44ab stepping 1 square North (44ab-45) vs. jumping 2 squares NE over B55e with optional whole-stack capture (44ab-66x).",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_05_multihop_chain.png",
                      "Figure 5: Multi-hop chain 33abd-55-57x-75x vaulting friendly A44h, pivoting North to capture B56e, then SE to capture B66c.",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_07_reserve_liberate.png",
                      "Figure 7: Deploying a free reserve pod onto empty home base 33 vs. liberating an own captured pod (53* or 57*) in 2P_3B* and 4P* once an enemy (or neutral 4P) OCTI square is occupied.",
                      figBg, lineHex, dimHex);
    os << "</td></tr></table>\n";

    /* Chapter 3 */
    os << "<a name=\"ch3\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>Chapter 3 &mdash; Stacking, Composite Moves &amp; Appendix A Edge Cases</h2>"
       << "<ul>"
       << "<li><b>Friendly Stacking &amp; Composite Multi-Pod Moves (<code>,</code>):</b> Multiple pods of your own color "
       << "may stack on the same square (forbidden in <code>OFK</code>; in <code>4P</code>, stacking with another color&mdash;including your diagonal partner&mdash;is never allowed). Any subset of your own pods that <b>start the turn "
       << "on the same square</b> may move together or split along separate step/jump paths in one turn, joined by commas "
       << "(e.g., <code>44a-45,44beg-34</code>). Pod placement and prong insertion never combine with movement.</li>"
       << "<li><b>Whole-Stack Capture &amp; End-of-Turn Removal:</b> Jumping over an occupied square with <code>x</code> captures "
       << "<b>every pod in that square's stack</b>. In regular single and composite moves, stacks jumped with <code>x</code> stay on the board as "
       << "jumpable fulcrums for remaining composite stack-mates during the same turn and are removed together at turn end. A single pod may "
       << "<b>never jump over the exact same square twice</b> during one hop chain.</li>"
       << "<li><b>Appendix A.1 &mdash; Starting-Square Vacancy, Loop-Back Jumps &amp; Stacked-Pod Non-Jumping:</b> For the duration of its "
       << "jump chain, a jumping pod <b>cannot jump over its starting square <code>S</code> or any pod with which it started stacked</b> "
       << "(so an Edgeless <code>_ED</code> wraparound jump such as <code>44bcd-66-86-16-34-54</code> over stationary <code>A44a</code> on "
       << "<code>44</code> is illegal). Furthermore, a jumping pod may <b>land back onto <code>S</code></b> at the end of a multi-hop loop "
       << "<b>only when all pods that started the turn on <code>S</code> vacate <code>S</code></b> during the move (for example, composite clear-out "
       << "<code>44a-45,44beg-66x-64x-44</code> is legal because <code>44</code> is completely vacated, whereas single <code>44beg-66x-64x-44</code> "
       << "leaving stationary <code>A44a</code> on <code>44</code> is illegal). Non-capturing circular multi-hop jumps (<code>S &rarr; ... &rarr; S</code> when "
       << "<code>S</code> is completely vacated) remain legal and count as movement-only plies (<code>+1</code> toward the no-progress draw threshold).</li>"
       << "<li><b>Pod Liberation Scope &amp; Clearance (<code>2P_3B*</code> &amp; <code>4P*</code>):</b> Liberating a captured pod (<code>xy*</code>) "
       << "applies exclusively to <b>3-Base (<code>2P_3B*</code>) and 4-Player (<code>4P*</code>)</b> games (never 1-Base sudden death or <code>OFK</code>), "
       << "always liberates 1 of <b>your own</b> captured pods (<code>capturedPods &gt; 0</code>), and always requires the target OCTI square "
       << "to be <b>already occupied by at least one of your own pods</b> (never an empty or partner-only square). In <code>2P_3B*</code>, liberating "
       << "onto your own home base additionally requires occupying at least one enemy base; liberating directly onto an enemy base you already occupy "
       << "requires only <code>capturedPods &gt; 0</code>. In <code>4P*</code>, liberating onto your own or partner's corner base requires personally "
       << "occupying a neutral (<code>53</code>/<code>57</code>) or enemy OCTI square (even if your partner is eliminated, <code>xy*</code> on their "
       << "corner base liberates your own pod and never restores an eliminated partner).</li>"
       << "<li><b>Edgeless Torus Wrapping (<code>_ED</code>):</b> Column <code>1</code> wraps to column <code>9</code> and row <code>1</code> wraps "
       << "to row <code>9</code> (including diagonals) for both steps (<code>15g-95</code>) and 2-square jumps (<code>97b-29x</code> jumping <code>B18e</code>).</li>"
       << "<li><b>SuperProngs &amp; Bonus Preludes (<code>_SP</code>, <code>;</code>):</b> A gold SuperProng (<code>!</code>) is permanent once inserted. "
       << "Before your regular turn action, you may execute <b>1 bonus prelude move per SuperProng you control on the board</b> (up to <b>2</b> in "
       << "<code>2P_SP</code> and up to <b>4</b> in <code>4P_SP</code> when enemy SuperProngs are captured), delimited by semicolons "
       << "(e.g., <code>44ab!-66x;66ab!-67</code>). Each prelude moves 1 pod strictly in a <b>straight line</b> (1-square step or collinear jump chain, "
       << "no pivoting) along a distinct SuperProng socket that has not yet fired a bonus prelude this turn. Stacks jumped with <code>x</code> during "
       << "a SuperProng bonus prelude are <b>removed immediately</b> after that prelude before the next SuperProng prelude or regular action begins! "
       << "A pod that moved in a bonus prelude may still use <b>any</b> of its prongs in the regular turn phase, and <b>victory is checked only at "
       << "completion of the full turn</b>.</li>"
       << "</ul>";
    appendFigureTable(os, "rule_06_stack_split_loop.png",
                      "Figure 6: Stack [A44a, A44beg] splitting via 44a-45,44beg-34 or clearing out 44 completely via 44a-45,44beg-66x-64x-44 so A44beg lands legally on vacated 44 (Appendix A.1).",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_08_edgeless_wrap.png",
                      "Figure 8: Edgeless (_ED) wrap-around step 15g-95 and diagonal wrap jump capture 97b-29x across the portal border.",
                      figBg, lineHex, dimHex);
    appendFigureTable(os, "rule_09_superprongs.png",
                      "Figure 9: Gold SuperProng straight-line bonus prelude 44ab!-66x; (1 bonus move per SuperProng controlled on the board) removing captured B55e! immediately before regular step 66ab!-67, with victory checked at full turn completion.",
                      figBg, lineHex, dimHex);
    os << "</td></tr></table>\n";

    /* Chapter 4 */
    os << "<a name=\"ch4\"></a>\n"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"12\" "
       << "style=\"margin-bottom:14px;border:1px solid " << lineHex
       << ";background-color:" << cardBg << ";\"><tr><td>"
       << "<h2>Chapter 4 &mdash; Victory, Elimination, Stalemate &amp; No-Progress Draw Conditions</h2>"
       << "<ul>"
       << "<li><b><code>2P_3B</code> Family (3-Base Standard):</b> Win at the completion of your full turn if your pods simultaneously occupy "
       << "<b>all 3</b> enemy home bases (<code>37</code>, <code>57</code>, <code>77</code> for A; <code>33</code>, <code>53</code>, <code>73</code> for B).</li>"
       << "<li><b><code>2P_1B</code> Family (1-Base Blitz):</b> Win at the completion of your full turn when your pod occupies <b>any 1</b> of the 3 enemy home bases.</li>"
       << "<li><b><code>OFK</code> (OCTI for Kids, 6&times;7):</b> Win at the completion of your full turn when your pod occupies <b>any 1</b> of the 4 enemy home bases "
       << "(<code>26, 36, 46, 56</code> for A; <code>22, 32, 42, 52</code> for B).</li>"
       << "<li><b><code>4P</code> Partnership Family (Team A&amp;C vs. Team B&amp;D):</b> Diagonal partners win together at the completion of a full turn when allied pods "
       << "simultaneously occupy <b>both enemy corner bases</b> (<code>37</code> &amp; <code>73</code> for A&amp;C; <code>33</code> &amp; <code>77</code> for B&amp;D).</li>"
       << "<li><b>4-Player Permanent Base-Capture Elimination &amp; Safe Partner Base:</b> In <code>4P</code>, if an <b>enemy</b> pod ends its full turn on your corner home "
       << "base (or all of your pods are captured with zero reserve pods remaining), you are <b>permanently eliminated</b> for the remainder of the game: "
       << "all of your remaining board pods are swept into your <code>capturedPods</code> pool, and <b>all prongs (normal and SuperProngs) from your "
       << "swept board pods transfer to the capturing enemy player</b>. Eliminated seats are skipped in turn rotation while a surviving partner plays on. "
       << "Conversely, <b>stepping or jumping onto your diagonal partner's empty (or self-occupied) starting base is legal and has no consequences&mdash;it never knocks your partner out of the game</b> "
       << "(if your partner temporarily occupies your starting base while you have zero pods on the board, your turn is simply skipped until they vacate it).</li>"
       << "<li><b>Total Wipeout / Stalemate:</b> Because prong repositioning is forbidden, a player with zero reserve pods and zero reserve prongs whose "
       << "board pods are completely boxed in has <b>zero legal moves</b>. If the next player in <code>2P</code> / <code>OFK</code> (or both enemy seats in "
       << "<code>4P</code>) has zero legal moves, the player/team who just moved wins immediately (in <code>4P</code>, a stalemated player whose partner "
       << "still has a legal move has their turn skipped). Timed matches also conclude on clock flag forfeit or resignation.</li>"
       << "<li><b>Universal No-Progress Draw (<code>NO_PROGRESS_DRAW</code>, <code>noProgressPlies</code>):</b> A match terminates automatically in a "
       << "<b>draw (<code>NO_PROGRESS_DRAW</code>)</b> when <b><code>30</code> consecutive plies (<code>15</code> full turns per player)</b> in any "
       << "<code>2P*</code> or <code>OFK</code> variant&mdash;or <b><code>60</code> consecutive plies (<code>15</code> full rounds)</b> in any "
       << "<code>4P*</code> variant&mdash;complete without any <b>progress event</b>: pod capture (<code>x</code>, including <code>_SP</code> bonus preludes), "
       << "reserve pod deployment (<code>xy</code>) or liberation (<code>xy*</code>), prong/SuperProng insertion (<code>+d</code>/<code>+d!</code>), or "
       << "permanent <code>4P</code> player elimination. Evaluated at turn completion <b>after</b> base-occupation victory, total capture wipeout, and "
       << "zero-legal-move stalemate checks (so winning on ply <code>30</code>/<code>60</code> still wins).</li>"
       << "</ul>";
    appendFigureTable(os, "rule_10_4p_partnership.png",
                      "Figure 10: 4-Player Partnership (4P): diagonal allies A(33)&C(77) vs. B(37)&D(73), own-color stack [A55ab, A55f], partner pod C65cf, safe partner-base entry, and neutral 53/57.",
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
       << "<tr><td><code>2P_3B_SP</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>SuperProng <code>!</code> + <code>;</code> straight bonus preludes (&le;2); 3 bases</td><td>Casual</td></tr>"
       << "<tr><td><code>2P_3B_ED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>Edgeless torus wrap-around; 3 bases</td><td>Casual</td></tr>"
       << "<tr><td><code>2P_3B_SPED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>SuperProng (&le;2 preludes) + Edgeless; 3 bases</td><td>Casual</td></tr>"
       << "<tr><td><code>2P_1B</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>Sudden-death blitz; any 1 enemy base</td><td><b>Yes</b></td></tr>"
       << "<tr><td><code>2P_1B_SP</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>SuperProng (&le;2 preludes) + 1-base sudden death</td><td>Casual</td></tr>"
       << "<tr><td><code>2P_1B_ED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>25 / 0</td><td>Edgeless + 1-base sudden death</td><td>Casual</td></tr>"
       << "<tr><td><code>2P_1B_SPED</code></td><td>9&times;9</td><td>2 (1v1)</td><td>3 + 4</td><td>24 / 1</td><td>SuperProng (&le;2 preludes) + Edgeless + 1-base</td><td>Casual</td></tr>"
       << "<tr><td><code>4P</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>12 / 0</td><td>Neutral <code>53/57</code>, permanent base elimination; 2 enemy bases</td><td>Casual</td></tr>"
       << "<tr><td><code>4P_SP</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>11 / 1</td><td>4P Partnership + SuperProng straight bonus preludes (&le;4)</td><td>Casual</td></tr>"
       << "<tr><td><code>4P_ED</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>12 / 0</td><td>4P Partnership + Edgeless wrap-around</td><td>Casual</td></tr>"
       << "<tr><td><code>4P_SPED</code></td><td>9&times;9</td><td>4 (A&amp;C vs B&amp;D)</td><td>1 + 3</td><td>11 / 1</td><td>4P Partnership + SuperProng (&le;4 preludes) + Edgeless</td><td>Casual</td></tr>"
       << "<tr><td><code>OFK</code></td><td>6&times;7</td><td>2 (1v1)</td><td>4 + 0</td><td>12 / 0</td><td>4 bases/side, 0 reserves, no stacking, no composite; 1 base</td><td><b>Yes</b></td></tr>"
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
       << "<tr><td><code>xy*</code></td><td>Liberate own captured pod onto occupied OCTI square (<code>2P_3B*</code> &amp; <code>4P*</code> only)</td><td><code>53*</code>, <code>57*</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;+&lt;d&gt;</code></td><td>Insert normal prong in socket <code>d &isin; {a..h}</code></td><td><code>55acg+b</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;+&lt;d&gt;!</code></td><td>Insert gold SuperProng in socket <code>d</code></td><td><code>33+b!</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;-x'y'</code></td><td>1-square non-capturing step</td><td><code>44ab-45</code></td></tr>"
       << "<tr><td><code>xy&lt;cfg&gt;-x1y1[x]-...</code></td><td>Single or multi-hop jump chain (<code>x</code> = optional whole-stack capture)</td><td><code>33abd-55-57x-75x</code></td></tr>"
       << "<tr><td><code>op1,op2[,...]</code></td><td>Composite move splitting pods from the same origin square</td><td><code>44a-45,44beg-66x-64x-44</code></td></tr>"
       << "<tr><td><code>pre1;[pre2;...]reg</code></td><td>1&ndash;4 straight SuperProng bonus preludes (1 per SP on board; immediate <code>x</code> removal) + regular turn</td><td><code>44ab!-66x;66ab!-67</code></td></tr>"
       << "</table>"
       << "<h3>OCTI-FEN Position String Format</h3>"
       << "<p>Board states serialize into colon-separated <b>OCTI-FEN</b> strings (11 tokens for <code>2</code> / <code>OFK</code>; "
       << "13 tokens for <code>4</code>):</p>"
       << "<p><code>mode:bases:super:edgeless:status:turn:winner:elimMask:resA:resB[:resC:resD]:squares[:noProgressPlies]</code></p>"
       << "<ul>"
       << "<li><code>mode</code>: <code>2</code>, <code>4</code>, or <code>OFK</code> &bull; <code>bases</code>: <code>3</code> or <code>1</code> "
       << "&bull; <code>super</code> / <code>edgeless</code>: <code>0</code> or <code>1</code>.</li>"
       << "<li><code>status</code>: <code>N</code> (not started), <code>P</code> (in progress), <code>O</code> (game over), <code>R</code> (resigned).</li>"
       << "<li><code>turn</code> / <code>winner</code>: <code>A</code>, <code>B</code>, <code>C</code>, <code>D</code>, or <code>-</code> "
       << "&bull; <code>elimMask</code>: 4P elimination bitmask (<code>A=1, B=2, C=4, D=8</code>).</li>"
       << "<li><code>resP</code>: <code>freePods,capturedPods,freeProngs,freeSuperProngs</code> (e.g., <code>4,0,25,0</code>).</li>"
       << "<li><code>squares</code>: comma-separated pods in row-major order <code>&lt;Owner&gt;&lt;x&gt;&lt;y&gt;&lt;prongs&gt;</code> "
       << "(e.g., <code>A44ab!,B55e!</code>), or <code>-</code> if empty.</li>"
       << "<li><code>noProgressPlies</code> (optional trailing 12th/14th token): consecutive non-progress plies (<code>0..1000</code>, defaults to <code>0</code> when omitted; canonical <code>toPositionString()</code> emits the 11/13-token string).</li>"
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
       << "Human-Only ratings are completely insulated from robot matches."
       << "</td></tr></table>"
       << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"9\" style=\"border-left:4px solid "
       << headHex << ";background-color:" << altBg << ";\"><tr><td>"
       << "<b>Pool 2 &mdash; Open Robot &amp; Human Arena (Leaderboard &amp; Robots Directory):</b><br>"
       << "Tracks rated online matches involving at least one registered AI robot (Human-vs-Robot and Robot-vs-Robot) "
       << "with three anti-farming safeguards:"
       << "<ul>"
       << "<li><b>Human-vs-Robot Matches:</b> Unlimited rated play at standard <code>K = 32</code> (including against your own robots).</li>"
       << "<li><b>Robot-vs-Robot K-Factors:</b> Cross-owner matches use standard <code>K = 32</code>; same-owner self-testing uses reduced <code>K = 16</code>.</li>"
       << "<li><b>Robot-vs-Robot Pair Cap &amp; Early-Resign Guard:</b> Robot-vs-Robot matches are capped at <b>4 rated games per rolling 24-hour window</b> per unordered robot pair "
       << "(5th+ auto-downgrades to casual; no daily cap across distinct opponent robots), and resignations before <b>8 plies</b> record 0 Elo delta.</li>"
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

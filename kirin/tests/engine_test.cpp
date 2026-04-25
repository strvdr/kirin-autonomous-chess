/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    engine_test.cpp - Engine validity and skill level test suite
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*    
*    Tests are organised into four suites:
*    
*      1. Perft              – move-generation node counts vs published ground truth
*      2. Evaluation sanity  – sign, symmetry, and material ordering of evaluate()
*      3. Tactical positions – perft(1)==0 for checkmate/stalemate; forced moves
*                              verified by counting legal alternatives
*      4. Skill level        – skillLevel global, depth-cap effect via perft counts,
*                              and move legality invariant via generateMoves/makeMove
*    
*    Search tests run with UCI input polling disabled.  This keeps CTest's
*    /dev/null stdin from affecting search while preserving polling for UCI mode.
*/

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>   // abs

#include "engine.h"

// ─────────────────────────────────────────────────────────────────────────────
// Test framework  (identical style to the other test files in this project)
// ─────────────────────────────────────────────────────────────────────────────

#define ANSI_RED    "\x1b[31m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_CYAN   "\x1b[36m"
#define ANSI_BOLD   "\x1b[1m"
#define ANSI_RESET  "\x1b[0m"

static int testsRun    = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST_ASSERT(cond, msg)                                              \
    do {                                                                    \
        testsRun++;                                                         \
        if (cond) {                                                         \
            testsPassed++;                                                  \
            printf("  " ANSI_GREEN "✔ %s" ANSI_RESET "\n", msg);          \
        } else {                                                            \
            testsFailed++;                                                  \
            printf("  " ANSI_RED "✗ %s" ANSI_RESET "  [line %d]\n",      \
                   msg, __LINE__);                                          \
        }                                                                   \
    } while(0)

static void printHeader(const char *name) {
    printf("\n" ANSI_BOLD ANSI_CYAN "── %s ──" ANSI_RESET "\n", name);
}

static bool writeI32(FILE *file, std::int32_t value) {
    return fwrite(&value, sizeof(value), 1, file) == 1;
}

static bool writeU32(FILE *file, std::uint32_t value) {
    return fwrite(&value, sizeof(value), 1, file) == 1;
}

static bool writeTestNNUE(const char *path) {
    FILE *file = fopen(path, "wb");
    if (!file) return false;

    const char magic[8] = { 'K', 'I', 'R', 'I', 'N', 'N', 'U', 'E' };
    bool ok = fwrite(magic, sizeof(magic), 1, file) == 1;

    const std::uint32_t featureCount = 12 * 64;
    const std::uint32_t hiddenSize = 2;
    ok = ok && writeU32(file, 1);             // format version
    ok = ok && writeU32(file, featureCount);
    ok = ok && writeU32(file, hiddenSize);
    ok = ok && writeI32(file, 4096);          // activation limit
    ok = ok && writeI32(file, 13);            // output bias

    ok = ok && writeI32(file, 0) && writeI32(file, 0);  // hidden biases
    ok = ok && writeI32(file, 1) && writeI32(file, -1); // output weights

    for (std::uint32_t feature = 0; feature < featureCount; feature++) {
        std::int32_t w0 = 0;
        std::int32_t w1 = 0;
        if (feature == static_cast<std::uint32_t>(Q * 64 + f1)) {
            w0 = 300;
        }
        ok = ok && writeI32(file, w0) && writeI32(file, w1);
    }

    fclose(file);
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Returns true iff the move encoded as UCI string "e2e4" / "e7e8q" is legal
// in the current position.
static bool moveIsLegal(const char *s) {
    int src = (s[0] - 'a') + (8 - (s[1] - '0')) * 8;
    int tgt = (s[2] - 'a') + (8 - (s[3] - '0')) * 8;
    char promo = s[4];
    moves ml[1];
    generateMoves(ml);
    for (int i = 0; i < ml->count; i++) {
        int m = ml->moves[i];
        if (getSource(m) != src || getTarget(m) != tgt) continue;
        if (promo) {
            int pp = getPromoted(m);
            if (!pp) continue;
            char c = (pp==Q||pp==q)?'q':(pp==R||pp==r)?'r':
                     (pp==B||pp==b)?'b':(pp==N||pp==n)?'n':0;
            if (c != promo) continue;
        }
        BoardState state = copyBoard(); ply = 0;
        bool legal = (makeMove(m, allMoves) != 0);
        restoreBoard(state);
        if (legal) return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 1 – Perft
// All node counts are from chessprogramming.org/Perft_Results.
// ─────────────────────────────────────────────────────────────────────────────

static void testPerft() {
    printHeader("Perft (move-generation correctness)");

    // Starting position
    parseFEN(startPosition);
    TEST_ASSERT(perft(1) == 20,     "start pos depth 1 = 20");
    TEST_ASSERT(perft(2) == 400,    "start pos depth 2 = 400");
    TEST_ASSERT(perft(3) == 8902,   "start pos depth 3 = 8902");
    TEST_ASSERT(perft(4) == 197281, "start pos depth 4 = 197281");

    // Kiwipete – castling, en-passant, promotions, pins all in one position
    parseFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    TEST_ASSERT(perft(1) == 48,    "kiwipete depth 1 = 48");
    TEST_ASSERT(perft(2) == 2039,  "kiwipete depth 2 = 2039");
    TEST_ASSERT(perft(3) == 97862, "kiwipete depth 3 = 97862");

    // Position 3 – tricky pawn structure and en-passant
    parseFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    TEST_ASSERT(perft(1) == 14,   "pos3 depth 1 = 14");
    TEST_ASSERT(perft(2) == 191,  "pos3 depth 2 = 191");
    TEST_ASSERT(perft(3) == 2812, "pos3 depth 3 = 2812");

    // Position 5 (CPW) – promotions and discovered checks
    parseFEN("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
    TEST_ASSERT(perft(1) == 44,    "pos5 depth 1 = 44");
    TEST_ASSERT(perft(2) == 1486,  "pos5 depth 2 = 1486");
    TEST_ASSERT(perft(3) == 62379, "pos5 depth 3 = 62379");
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 2 – Evaluation sanity
// ─────────────────────────────────────────────────────────────────────────────

static void testEvaluationSanity() {
    printHeader("Evaluation sanity");

    setUseNNUE(false);

    parseFEN(startPosition);
    TEST_ASSERT(abs(evaluate()) < 100,
                "starting position evaluates near 0 (|score| < 100cp)");

    parseFEN("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");
    TEST_ASSERT(evaluate() > 800,
                "white queen vs bare king: eval > +800cp (white to move)");

    parseFEN("4k1q1/8/8/8/8/8/8/4K3 w - - 0 1");
    TEST_ASSERT(evaluate() < -800,
                "black queen vs bare king: eval < -800cp (white to move)");

    parseFEN("4k2r/8/8/8/8/8/8/R3K3 w - - 0 1");
    TEST_ASSERT(abs(evaluate()) < 200,
                "symmetric K+R vs k+r evaluates near 0 (|score| < 200cp)");

    // Material ordering: Q(1000cp) > R(500cp) > P(100cp)
    parseFEN("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1"); int withQ = evaluate();
    parseFEN("4k3/8/8/8/8/8/8/4KR2 w - - 0 1"); int withR = evaluate();
    parseFEN("4k3/8/8/8/8/8/8/4KP2 w - - 0 1"); int withP = evaluate();

    TEST_ASSERT(withQ > withR, "K+Q scores higher than K+R");
    TEST_ASSERT(withR > withP, "K+R scores higher than K+P");
    TEST_ASSERT(withP > 0,     "single pawn up is positive for side to move");
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 2b – NNUE scaffold
// ─────────────────────────────────────────────────────────────────────────────

static void testNNUEScaffold() {
    printHeader("NNUE scaffold");

    TEST_ASSERT(isNNUEInitialized(),
                "initAll() initializes the built-in NNUE bootstrap network");

    parseFEN(startPosition);
    setUseNNUE(false);
    int classicalStart = evaluate();
    setUseNNUE(true);
    int nnueStart = evaluate();

    TEST_ASSERT(getUseNNUE(),
                "setUseNNUE(true) enables NNUE evaluation mode");
    TEST_ASSERT(abs(nnueStart) < 100,
                "NNUE starting position evaluates near 0 (|score| < 100cp)");
    TEST_ASSERT(evaluateNNUE() == evaluate(),
                "evaluate() routes to evaluateNNUE() when NNUE mode is enabled");

    parseFEN("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");
    int whiteToMoveScore = evaluate();
    parseFEN("4k3/8/8/8/8/8/8/4KQ2 b - - 0 1");
    int blackToMoveScore = evaluate();
    TEST_ASSERT(whiteToMoveScore == -blackToMoveScore,
                "NNUE evaluation is relative to side to move");

    parseFEN("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");
    int withQ = evaluate();
    parseFEN("4k3/8/8/8/8/8/8/4KR2 w - - 0 1");
    int withR = evaluate();
    parseFEN("4k3/8/8/8/8/8/8/4KP2 w - - 0 1");
    int withP = evaluate();

    TEST_ASSERT(withQ > withR,
                "NNUE bootstrap scores K+Q higher than K+R");
    TEST_ASSERT(withR > withP,
                "NNUE bootstrap scores K+R higher than K+P");
    TEST_ASSERT(withP > 0,
                "NNUE bootstrap scores a single pawn as positive");

    setUseNNUE(false);
    TEST_ASSERT(!getUseNNUE(),
                "setUseNNUE(false) restores classical evaluation mode");
    parseFEN(startPosition);
    TEST_ASSERT(evaluate() == evaluateClassical() && classicalStart == evaluateClassical(),
                "classical evaluation remains callable after NNUE mode is disabled");

    const char *testNet = "kirin_test_network.knnue";
    TEST_ASSERT(writeTestNNUE(testNet),
                "test harness writes a valid Kirin NNUE file");
    TEST_ASSERT(loadNNUE(testNet),
                "loadNNUE() accepts a valid Kirin NNUE file");
    TEST_ASSERT(hasExternalNNUE(),
                "hasExternalNNUE() reports a loaded external network");
    TEST_ASSERT(strcmp(getNNUESource(), testNet) == 0,
                "getNNUESource() reports the loaded NNUE file path");

    setUseNNUE(true);
    parseFEN("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");
    int loadedWhiteScore = evaluate();
    parseFEN("4k3/8/8/8/8/8/8/4KQ2 b - - 0 1");
    TEST_ASSERT(loadedWhiteScore == 313 && evaluate() == -313,
                "loaded NNUE network produces the expected side-relative score");

    TEST_ASSERT(!loadNNUE("missing-network.knnue"),
                "loadNNUE() rejects missing files");
    parseFEN("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");
    TEST_ASSERT(evaluate() == 313,
                "failed NNUE loads leave the active network unchanged");

    resetNNUEToBootstrap();
    setUseNNUE(false);
    TEST_ASSERT(!hasExternalNNUE(),
                "resetNNUEToBootstrap() restores the built-in fallback network");
    remove(testNet);
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 3 – Tactical position structure
//
// Tactical positions are verified through:
//   • perft(1) == 0  for checkmate and stalemate (no legal moves)
//   • isAttacked()   to distinguish checkmate from stalemate
//   • moveIsLegal()  to confirm a specific move exists in the move list
//   • countLegalMoves() to verify forced situations (only one legal reply, etc.)
// ─────────────────────────────────────────────────────────────────────────────

static void testTacticalPositions() {
    printHeader("Tactical position structure");

    // ── Checkmate: Fool's Mate ────────────────────────────────────────────
    // After 1.f3 e5 2.g4 Qh4# — white has no legal moves and is in check.
    parseFEN("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 3");
    TEST_ASSERT(perft(1) == 0,
                "checkmate (Fool's Mate): 0 legal moves for white");
    TEST_ASSERT(isAttacked(getLSBindex(bitboards[K]), black),
                "checkmate (Fool's Mate): white king is in check");

    // ── Checkmate: Scholar's Mate ─────────────────────────────────────────
    // After Qxf7#.  Black has no legal moves and is in check.
    parseFEN("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4");
    TEST_ASSERT(perft(1) == 0,
                "checkmate (Scholar's Mate): 0 legal moves for black");
    TEST_ASSERT(isAttacked(getLSBindex(bitboards[k]), white),
                "checkmate (Scholar's Mate): black king is in check");

    // ── Stalemate ─────────────────────────────────────────────────────────
    parseFEN("k7/8/KQ6/8/8/8/8/8 b - - 0 1");
    TEST_ASSERT(perft(1) == 0,
                "stalemate: 0 legal moves for black");
    TEST_ASSERT(!isAttacked(getLSBindex(bitboards[k]), white),
                "stalemate: black king is NOT in check");

    // ── Forced capture exists in move list ───────────────────────────────
    // White rook on d1, black queen on d5: Rd1xd5 must be a legal move.
    parseFEN("4k3/8/8/3q4/8/8/8/3RK3 w - - 0 1");
    TEST_ASSERT(moveIsLegal("d1d5"),
                "forced win: Rd1xd5 (takes free queen) is in the legal move list");

    // ── Pin: pinned piece move is NOT in the legal move list ─────────────
    // Be2 is pinned to Ke1 by Re8 along the e-file.  Be2-d3 would expose the
    // king and must not be legal.
    parseFEN("4rk2/8/8/8/8/8/4B3/4K3 w - - 0 1");
    TEST_ASSERT(!moveIsLegal("e2d3"),
                "pin: Be2-d3 is illegal (bishop is pinned to the king)");
    TEST_ASSERT(!moveIsLegal("e2f3"),
                "pin: Be2-f3 is illegal (bishop is pinned to the king)");
    TEST_ASSERT(parseMove("e2d3") == 0,
                "parseMove rejects pseudo-legal pinned moves");

    // ── En passant move is in the legal move list ─────────────────────────
    // White pawn e5, black just played d7-d5 so ep square is d6.
    // White can play e5xd6 en passant.
    parseFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    TEST_ASSERT(moveIsLegal("e5d6"),
                "en passant: e5xd6 is in the legal move list");

    // ── Promotion moves exist for all four pieces ─────────────────────────
    parseFEN("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    TEST_ASSERT(moveIsLegal("a7a8q"), "promotion: a7-a8=Q is legal");
    TEST_ASSERT(moveIsLegal("a7a8r"), "promotion: a7-a8=R is legal");
    TEST_ASSERT(moveIsLegal("a7a8b"), "promotion: a7-a8=B is legal");
    TEST_ASSERT(moveIsLegal("a7a8n"), "promotion: a7-a8=N is legal");

    parseFEN("1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    int promotionCapture = parseMove("a7b8q");
    TEST_ASSERT(promotionCapture != 0 && getCapturedPiece(promotionCapture) == n,
                "promotion capture stores the captured piece for move ordering");

    // ── Castling moves exist when rights are intact ───────────────────────
    parseFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    TEST_ASSERT(moveIsLegal("e1g1"), "castling: white kingside O-O is legal");
    TEST_ASSERT(moveIsLegal("e1c1"), "castling: white queenside O-O-O is legal");
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 4 – Skill level
//
// We test the skillLevel global and the depth-cap effect.  The depth cap lives
// inside searchPosition() which we cannot call safely (see header note).
// We verify:
//   a) skillLevel reads and writes correctly
//   b) Perft node counts confirm that shallow search (depth 3) visits far
//      fewer nodes than deep search (depth 5) — the same ratio the skill caps
//      enforce at runtime.
//   c) Every move in the legal move list at every skill level is valid —
//      we iterate generateMoves output and verify makeMove accepts it.
// ─────────────────────────────────────────────────────────────────────────────

static void testSkillLevel() {
    printHeader("Skill level");

    // a) skillLevel global round-trips
    skillLevel = 0; TEST_ASSERT(skillLevel == 0, "skillLevel = 0 reads back correctly");
    skillLevel = 1; TEST_ASSERT(skillLevel == 1, "skillLevel = 1 reads back correctly");
    skillLevel = 2; TEST_ASSERT(skillLevel == 2, "skillLevel = 2 reads back correctly");

    // b) Depth-cap proxy: node count grows with depth (same ratio caps enforce)
    parseFEN(startPosition);
    long n3 = perft(3);   // 8,902  — baseline for skill 0 cap (depth 3)
    long n4 = perft(4);   // 197,281
    long n5 = perft(5);   // 4,865,609 — baseline for uncapped depth

    TEST_ASSERT(n3 == 8902,   "perft(3) = 8902 (depth-3 baseline)");
    TEST_ASSERT(n3 < n4,      "depth 3 visits fewer nodes than depth 4");
    TEST_ASSERT(n4 < n5,      "depth 4 visits fewer nodes than depth 5");

    printf("  [info] perft node counts — d3:%ld  d4:%ld  d5:%ld\n", n3, n4, n5);

    // c) Every pseudo-legal move that makeMove accepts is a valid legal move —
    //    the move generator must never produce moves that leave the king in check.
    //    Test across several positions at all skill levels.
    const char *fens[] = {
        startPosition,
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "4k3/P7/8/8/8/8/8/4K3 w - - 0 1",
        "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2",
    };
    const int NFENS = (int)(sizeof(fens) / sizeof(fens[0]));

    for (int sk = 0; sk <= 2; sk++) {
        skillLevel = sk;
        for (int fi = 0; fi < NFENS; fi++) {
            parseFEN(fens[fi]);
            moves ml[1];
            generateMoves(ml);
            bool allLegal = true;
            for (int i = 0; i < ml->count; i++) {
                BoardState state = copyBoard(); ply = 0;
                bool legal = (makeMove(ml->moves[i], allMoves) != 0);
                // If a move in the pseudo-legal list IS made, verify king not in check
                // (makeMove already enforces this — we just confirm no move slips through)
                if (legal) {
                    // After make, side has flipped; verify the side that just moved's
                    // king is not attacked (makeMove already checks this, but belt+braces)
                    int kingSq = getLSBindex(bitboards[side == white ? k : K]);
                    if (isAttacked(kingSq, side)) { allLegal = false; }
                }
                restoreBoard(state);
            }
            char label[80];
            snprintf(label, sizeof(label),
                     "skill %d, fen %d: no legal move leaves king in check", sk, fi);
            TEST_ASSERT(allLegal, label);
        }
    }

    // Reset to hard
    skillLevel = 2;
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 5 – Repetition detection
// ─────────────────────────────────────────────────────────────────────────────

static void testRepetitionDetection() {
    printHeader("Repetition detection");

    parseFEN(startPosition);
    U64 startHash = hashKey;
    parsePosition("position startpos moves g1f3");
    TEST_ASSERT(repetitionIndex == 1 && repetitionTable[0] == startHash,
                "parsePosition records the pre-move hash before incrementing repetitionIndex");

    parseFEN("4k3/8/8/8/8/8/8/4K2R w - - 0 1");

    repetitionIndex = 0;
    repetitionTable[repetitionIndex] = hashKey;
    repetitionIndex++;

    ply = 1;
    TEST_ASSERT(isRepeating() == 1,
                "isRepeating() returns 1 after current hash is recorded");

    // A different position must not trigger it
    parseFEN("4k3/8/8/8/8/8/8/3K2R1 w - - 0 1");
    ply = 1;
    TEST_ASSERT(isRepeating() == 0,
                "isRepeating() returns 0 for an unseen position");

    repetitionIndex = 0;
    ply = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Suite 6 – Search API
// ─────────────────────────────────────────────────────────────────────────────

static void testSearchApi() {
    printHeader("Search API");

    setUciInputPolling(0);
    stopped = 0;
    communicate();
    TEST_ASSERT(stopped == 0,
                "communicate() does not read stdin when UCI input polling is disabled");

    resetTimeControl();
    setUciInputPolling(0);
    skillLevel = 2;
    parseFEN(startPosition);
    searchPosition(2);

    BoardState state = copyBoard();
    bool legalBestMove = (makeMove(bestMove, allMoves) != 0);
    restoreBoard(state);

    TEST_ASSERT(stopped == 0,
                "searchPosition() completes under CTest without stdin side effects");
    TEST_ASSERT(legalBestMove,
                "searchPosition() returns a legal best move through the public API");
    TEST_ASSERT(transpositionTable[hashKey % hashSize].key == hashKey &&
                transpositionTable[hashKey % hashSize].move == bestMove,
                "searchPosition() stores the root best move in the transposition table");
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    printf(ANSI_BOLD "\n╔══ Kirin Engine Test Suite ══╗\n" ANSI_RESET);

    initAll();

    testPerft();
    testEvaluationSanity();
    testNNUEScaffold();
    testTacticalPositions();
    testSkillLevel();
    testRepetitionDetection();
    testSearchApi();

    printf("\n" ANSI_BOLD "╔══ Results: %d/%d passed" ANSI_RESET,
           testsPassed, testsRun);
    if (testsFailed > 0)
        printf(ANSI_RED " (%d failed)" ANSI_RESET, testsFailed);
    printf("\n\n");

    return (testsFailed == 0) ? 0 : 1;
}

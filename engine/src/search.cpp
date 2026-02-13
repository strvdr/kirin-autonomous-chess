/*
 * Kirin Chess Engine
 * search.cpp - Search algorithms and transposition table
 */

#include <cstdio>
#include <cstring>
#include "search.h"
#include "bitboard.h"
#include "movegen.h"
#include "evaluation.h"
#include "attacks.h"
#include "zobrist.h"
#include "utils.h"
#include "position.h"

/************ Transposition Table ************/
tt transpositionTable[hashSize];

/************ Search Tables ************/
int killerMoves[2][maxPly];
int historyMoves[12][64];
int pvLength[maxPly];
int pvTable[maxPly][maxPly];
int followPV = 0;
int scorePV = 0;

/************ MVV-LVA Table ************/
/*
                          
    (Victims) Pawn Knight Bishop   Rook  Queen   King
  (Attackers)
        Pawn   105    205    305    405    505    605
      Knight   104    204    304    404    504    604
      Bishop   103    203    303    403    503    603
        Rook   102    202    302    402    502    602
       Queen   101    201    301    401    501    601
        King   100    200    300    400    500    600

*/
int mvvLVA[12][12] = {
    105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
    104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
    103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
    102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
    101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
    100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600,
    
    105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
    104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
    103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
    102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
    101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
    100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600
};

/************ Late Move Reduction Constants ************/
static const int fullDepthMoves = 4;
static const int reductionLimit = 3;

/************ Transposition Table Functions ************/
void clearTranspositionTable() { 
    for (int index = 0; index < hashSize; index++) { 
        transpositionTable[index].key = 0;
        transpositionTable[index].depth = 0;
        transpositionTable[index].flag = 0;
        transpositionTable[index].score = 0;
    }
}

int readHashEntry(int alpha, int beta, int depth) { 
    tt *hashEntry = &transpositionTable[hashKey % hashSize];
    
    if (hashEntry->key == hashKey) { 
        if (hashEntry->depth >= depth) {
            int score = hashEntry->score;
            
            if (score < -mateScore) score += ply;
            if (score > mateScore) score -= ply;
            
            if (hashEntry->flag == hashFlagExact) { 
                return score;
            }
            if ((hashEntry->flag == hashFlagAlpha) && (score <= alpha)) { 
                return alpha;
            }
            if ((hashEntry->flag == hashFlagBeta) && (score >= beta)) { 
                return beta;
            }
        }
    }
    return noHashEntry;
}

void recordHash(int score, int depth, int hashFlag) {
    tt *hashEntry = &transpositionTable[hashKey % hashSize];
    
    if (score < -mateScore) score -= ply;
    if (score > mateScore) score += ply;
    
    hashEntry->key = hashKey;
    hashEntry->depth = depth;
    hashEntry->flag = hashFlag;
    hashEntry->score = score;
}

/************ Move Ordering ************/
void enablePvScoring(moves *moveList) {
    followPV = 0; 
    for (int i = 0; i < moveList->count; i++) { 
        if (pvTable[0][ply] == moveList->moves[i]) {
            scorePV = 1;
            followPV = 1;
        }
    }
}

/*
 * Move ordering priorities:
 * 1. PV move
 * 2. Captures (MVV/LVA)
 * 3. 1st Killer Move
 * 4. 2nd Killer Move
 * 5. History Moves
 * 6. Unsorted Moves
 */
int scoreMove(int move) { 
    // PV move
    if (scorePV) { 
        if (pvTable[0][ply] == move) {
            scorePV = 0;
            return 20000;
        }
    }
    
    // Captures
    if (getCapture(move)) { 
        int targetPiece = P;
        int startPiece, endPiece;
        
        if (side == white) { 
            startPiece = p; 
            endPiece = k; 
        } else { 
            startPiece = P; 
            endPiece = K; 
        }
        
        for (int bbPiece = startPiece; bbPiece <= endPiece; bbPiece++) { 
            if (getBit(bitboards[bbPiece], getTarget(move))) {
                targetPiece = bbPiece;
                break;
            }
        }
        return mvvLVA[getPiece(move)][targetPiece] + 10000;
    }
    
    // Quiet moves
    if (killerMoves[0][ply] == move) return 9000;   
    if (killerMoves[1][ply] == move) return 8000;   
    return historyMoves[getPiece(move)][getTarget(move)];
}

void sortMoves(moves *moveList) { 
    int moveScores[moveList->count]; 
    
    for (int i = 0; i < moveList->count; i++) { 
        moveScores[i] = scoreMove(moveList->moves[i]);
    }
    
    for (int current = 0; current < moveList->count; current++) { 
        for (int next = current + 1; next < moveList->count; next++) { 
            if (moveScores[current] < moveScores[next]) { 
                // Swap scores
                int tempScore = moveScores[current];
                moveScores[current] = moveScores[next];
                moveScores[next] = tempScore;
                
                // Swap moves
                int tempMove = moveList->moves[current];
                moveList->moves[current] = moveList->moves[next];
                moveList->moves[next] = tempMove;
            }
        } 
    }
}

void printMoveScores(moves *moveList) { 
    printf("Move Scores\n\n");
    for (int i = 0; i < moveList->count; i++) { 
        printf("    move: "); 
        printMove(moveList->moves[i]);
        printf(" score: %d\n", scoreMove(moveList->moves[i]));
    }
}

/************ Repetition Detection ************/
int isRepeating() { 
    for (int i = 0; i < repetitionIndex; i++) { 
        if (repetitionTable[i] == hashKey) {
            return 1;
        }
    }
    return 0;
}

/************ Quiescence Search ************/
int quiescence(int alpha, int beta) { 
    if ((nodes & 2047) == 0) communicate();
    
    nodes++;
    
    if (ply > maxPly - 1) return evaluate();
    
    int eval = evaluate();
    if (eval >= beta) return beta;
    if (eval > alpha) alpha = eval;
    
    moves moveList[1]; 
    generateMoves(moveList);
    sortMoves(moveList);
    
    for (int i = 0; i < moveList->count; i++) {
        copyBoard();
        ply++;
        
        repetitionTable[repetitionIndex] = hashKey;
        repetitionIndex++;
        
        if (makeMove(moveList->moves[i], onlyCaptures) == 0) { 
            ply--;
            repetitionIndex--;
            continue;
        }
        
        int score = -quiescence(-beta, -alpha);
        
        ply--;
        repetitionIndex--;
        restoreBoard();
        
        if (stopped == 1) return 0;
        
        if (score > alpha) {
            alpha = score;
            if (score >= beta) return beta;
        }
    }
    
    return alpha;
}

/************ Negamax Alpha-Beta Search ************/
int negamax(int alpha, int beta, int depth) { 
    int score;
    int hashFlag = hashFlagAlpha;
    int pvNode = (beta - alpha > 1);
    
    // Check for repetition
    if (ply && isRepeating()) return 0;
    
    // Probe transposition table
    if (ply && (score = readHashEntry(alpha, beta, depth)) != noHashEntry && !pvNode) {
        return score; 
    }
    
    if ((nodes & 2047) == 0) communicate();
    
    pvLength[ply] = ply;
    
    // Recursion escape
    if (depth == 0) return quiescence(alpha, beta);
    
    // Check for overflow
    if (ply > maxPly - 1) return evaluate();
    
    nodes++; 
    
    int inCheck = isAttacked(
        (side == white) ? getLSBindex(bitboards[K]) : getLSBindex(bitboards[k]), 
        side ^ 1
    );
    
    // Increase search depth if in check
    if (inCheck) depth++;
    
    int legalMoves = 0;
    
    // Null move pruning
    if (depth >= 3 && inCheck == 0 && ply) {
        copyBoard();
        ply++;
        
        repetitionTable[repetitionIndex] = hashKey;
        repetitionIndex++;
        
        if (enpassant != no_sq) hashKey ^= enpassantKeys[enpassant];
        enpassant = no_sq;
        side ^= 1;
        hashKey ^= sideKey;
        
        score = -negamax(-beta, -beta + 1, depth - 3);
        
        ply--;
        repetitionIndex--;
        restoreBoard();
        
        if (stopped == 1) return 0;
        if (score >= beta) return beta;
    }
    
    moves moveList[1]; 
    generateMoves(moveList);
    
    if (followPV) enablePvScoring(moveList);
    
    sortMoves(moveList);
    
    int movesSearched = 0;
    
    for (int i = 0; i < moveList->count; i++) {
        copyBoard();
        ply++;
        
        repetitionTable[repetitionIndex] = hashKey;
        repetitionIndex++;
        
        if (makeMove(moveList->moves[i], allMoves) == 0) { 
            ply--;
            repetitionIndex--;
            continue;
        }
        
        legalMoves++;
        
        // Principal Variation Search with Late Move Reduction
        if (movesSearched == 0) {
            score = -negamax(-beta, -alpha, depth - 1);
        } else {
            // Late move reduction
            if (movesSearched >= fullDepthMoves 
                && depth >= reductionLimit 
                && inCheck == 0 
                && getCapture(moveList->moves[i]) == 0 
                && getPromoted(moveList->moves[i]) == 0) {
                score = -negamax(-alpha - 1, -alpha, depth - 2);
            } else {
                score = alpha + 1;
            }
            
            // PVS
            if (score > alpha) { 
                score = -negamax(-alpha - 1, -alpha, depth - 1);
                if ((score > alpha) && (score < beta)) {
                    score = -negamax(-beta, -alpha, depth - 1);
                }
            }
        }
        
        ply--;
        repetitionIndex--;
        restoreBoard();
        
        if (stopped == 1) return 0;
        
        movesSearched++; 
        
        // Found a better move
        if (score > alpha) {
            hashFlag = hashFlagExact;
            
            // Update history for quiet moves
            if (getCapture(moveList->moves[i]) == 0) { 
                historyMoves[getPiece(moveList->moves[i])][getTarget(moveList->moves[i])] += depth;
            }
            
            alpha = score;
            
            // Update PV
            pvTable[ply][ply] = moveList->moves[i];
            for (int nextPly = ply + 1; nextPly < pvLength[ply + 1]; nextPly++) {
                pvTable[ply][nextPly] = pvTable[ply + 1][nextPly];
            }
            pvLength[ply] = pvLength[ply + 1];
            
            // Beta cutoff
            if (score >= beta) { 
                recordHash(beta, depth, hashFlagBeta);
                
                // Update killer moves for quiet moves
                if (getCapture(moveList->moves[i]) == 0) { 
                    killerMoves[1][ply] = killerMoves[0][ply];
                    killerMoves[0][ply] = moveList->moves[i];
                }
                return beta;
            }
        }
    }
    
    // No legal moves
    if (legalMoves == 0) {
        if (inCheck) {
            return -mateValue + ply;  // Checkmate
        }
        return 0;  // Stalemate
    }
    
    recordHash(alpha, depth, hashFlag);
    return alpha;
}

/************ Iterative Deepening Search ************/
void searchPosition(int depth) { 
    int score = 0;
    nodes = 0;
    stopped = 0;
    followPV = 0;
    scorePV = 0; 
    
    // Clear search data structures
    memset(killerMoves, 0, sizeof(killerMoves));
    memset(historyMoves, 0, sizeof(historyMoves));
    memset(pvTable, 0, sizeof(pvTable));
    memset(pvLength, 0, sizeof(pvLength));
    
    int alpha = -infinity;
    int beta = infinity;
    
    // Iterative deepening
    for (int currentDepth = 1; currentDepth <= depth; currentDepth++) { 
        if (stopped == 1) break;
        
        followPV = 1;
        score = negamax(alpha, beta, currentDepth);
        
        // Aspiration window failed - research with full window
        if ((score <= alpha) || (score >= beta)) {
            alpha = -infinity;
            beta = infinity;
            continue;
        }
        
        // Setup window for next iteration
        alpha = score - 50;
        beta = score + 50;
        
        // Print search info
        if (score > -mateValue && score < -mateScore) {
            printf("info score mate %d depth %d nodes %ld time %d pv ", 
                   -(score + mateValue) / 2 - 1, currentDepth, nodes, getTimeMS() - startTime);
        } else if (score > mateScore && score < mateValue) {
            printf("info score mate %d depth %d nodes %ld time %d pv ", 
                   (mateValue - score) / 2 + 1, currentDepth, nodes, getTimeMS() - startTime);   
        } else {
            printf("info score cp %d depth %d nodes %ld time %d pv ", 
                   score, currentDepth, nodes, getTimeMS() - startTime);
        }
        
        // Print PV line
        for (int i = 0; i < pvLength[0]; i++) {
            printMove(pvTable[0][i]);
            printf(" ");
        }
        printf("\n");
    }
    
    // Print best move
    printf("bestmove ");
    printMove(pvTable[0][0]);
    printf("\n");
    fflush(stdout);
}

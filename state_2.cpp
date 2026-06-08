#if defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <climits>
#include <chrono>
#include <random>
#include <memory>
#include "state.h"
#include "magic.h"
#include "pst.h"
#include <chrono>
// #include <cstring>

const uint64_t RANK_7_MASK = 0X00ff000000000000;
const uint64_t RANK_5_MASK = 0X000000ff00000000;
const uint64_t RANK_4_MASK = 0X00000000ff000000;
const uint64_t RANK_2_MASK = 0X000000000000ff00;

const uint64_t NOT_1_RANK = 0xFFFFFFFFFFFFFF00;
const uint64_t NOT_8_RANK = 0x00ffffffffffffff;
const uint64_t NOT_A_FILE = 0xFEFEFEFEFEFEFEFE;
const uint64_t NOT_H_FILE = 0x7F7F7F7F7F7F7F7F;
const uint64_t NOT_AB_FILE = 0xFCFCFCFCFCFCFCFC;
const uint64_t NOT_GH_FILE = 0x3F3F3F3F3F3F3F3F;

uint64_t black_pawn_attacks[64]; // pawn pushes are calculated on the fly
uint64_t white_pawn_attacks[64]; // pawn captures are pre-calculated
uint64_t knight_attacks[64];
uint64_t bishop_mask[64];
uint64_t bishop_attacks[64][512];
uint64_t rook_mask[64];
uint64_t rook_attacks[64][4096];
uint64_t queen_attacks[64];
uint64_t king_attacks[64];

// zhash
uint64_t piece_keys[12][64];
uint64_t enpassant_keys[64]; // 64 possible ep sq (though only 16 are practically used, 64 sq is easier to index) only 3rd and 6th ranks
uint64_t castle_keys[16];
uint64_t side_key;
std::mt19937_64 rng(31415);

int lmr_table[64][512];

FORCE_INLINE void set_bit(uint64_t &bitboard, int square)
{
    bitboard |= (1LL << square);
}
FORCE_INLINE void clear_bit(uint64_t &bitboard, int square)
{
    bitboard &= ~(1LL << square);
}
FORCE_INLINE bool get_bit(uint64_t &bitboard, int square)
{
    return bitboard & (1LL << square);
}

enum Square
{
    a1,b1,c1,d1,e1,f1,g1,h1,
    a2,b2,c2,d2,e2,f2,g2,h2,
    a3,b3,c3,d3,e3,f3,g3,h3,
    a4,b4,c4,d4,e4,f4,g4,h4,
    a5,b5,c5,d5,e5,f5,g5,h5,
    a6,b6,c6,d6,e6,f6,g6,h6,
    a7,b7,c7,d7,e7,f7,g7,h7,
    a8,b8,c8,d8,e8,f8,g8,h8,
    no_sq
};

const int castling_rights_update[64] = {
    11, 15, 15, 15, 3, 15, 15, 7,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    14, 15, 15, 15, 12, 15, 15, 13};

const int piece_values[12] = {
    // P,   N,   B,   R,   Q, K
    100, 300, 300, 500, 900, 0, // White
    100, 300, 300, 500, 900, 0  // Black
};

const int phase_piece_value[12] = {
    // P, N, B, R, Q, K
    0, 1, 1, 2, 4, 0, // White
    0, 1, 1, 2, 4, 0  // Black
};

enum Piece
{
    WHITE_PAWN,
    WHITE_KNIGHT,
    WHITE_BISHOP,
    WHITE_ROOK,
    WHITE_QUEEN,
    WHITE_KING,
    BLACK_PAWN,
    BLACK_KNIGHT,
    BLACK_BISHOP,
    BLACK_ROOK,
    BLACK_QUEEN,
    BLACK_KING
};

enum MoveFlags
{
    STANDARD_QUIET_MOVE = 0,
    STANDARD_CAPTURE = 1,
    KNIGHT_PROMOTION = 2,
    BISHOP_PROMOTION = 3,
    ROOK_PROMOTION = 4,
    QUEEN_PROMOTION = 5,
    KNIGHT_PROMOTION_AND_CAPTURE = 6,
    BISHOP_PROMOTION_AND_CAPTURE = 7,
    ROOK_PROMOTION_AND_CAPTURE = 8,
    QUEEN_PROMOTION_AND_CAPTURE = 9,
    EN_PASSANT = 10,
    KINGSIDE_CASTLE = 11,
    QUEENSIDE_CASTLE = 12
};

struct UnmakeInfo
{
    int castling_right;
    int ep_square;
    int captured_piece;
    int mg_eval;
    int eg_eval;
    int game_phase;
    uint64_t hash = 0;
};

struct Move
{
    uint16_t move;     // 2
    int32_t score = 0; // 4
};

struct MoveList
{
    Move moves_list[256];    // 6
    uint16_t move_count = 0; // 2
};

const int HASH_EXACT = 0;
const int HASH_ALPHA = 1;
const int HASH_BETA = 2;

struct TTentry
{
    uint64_t hash = 0; // 8
    int32_t score;     // 4
    uint16_t best_move;// 2 
    uint8_t depth;     // 1
    uint8_t flag;      // 1
};

constexpr int TTsize = 67108864; // sweetspot
std::unique_ptr<TTentry[]> transposition_table(new TTentry[TTsize]);

// TTentry *transposition_table = new TTentry[10000000];

void clear_TT()
{
    for (size_t i = 0; i < TTsize; ++i)
    {
        transposition_table[i].hash = 0;
    }
    //std::memset
    //(
    //    transposition_table.get(),
    //    0,
    //    TTsize * sizeof(TTentry)
    //);
}

FORCE_INLINE void tt_replace(uint32_t tt_hash, TTentry &entry)
{
    if (transposition_table[tt_hash].hash == entry.hash) 
    {
        if (entry.best_move == 0) {
            entry.best_move = transposition_table[tt_hash].best_move;
        }
        if (transposition_table[tt_hash].depth <= entry.depth) {
            transposition_table[tt_hash] = entry;
        } 
        else 
        {
            // don't overwrite the deep score but update the PV move
            transposition_table[tt_hash].best_move = entry.best_move;
        }
        // transposition_table[tt_hash] = entry;
    }
    else if (transposition_table[tt_hash].depth <= entry.depth)
    {
        transposition_table[tt_hash] = entry;

    }
}

bool stop_search = false;
std::chrono::_V2::system_clock::time_point timer_start;
std::chrono::duration<int64_t, std::milli> soft_time;
std::chrono::duration<int64_t, std::milli> hard_time;
int NODES = 0;
class GameState
{
public:
    void fen_parser(std::string fen)
    {
        int rank = 7;
        int file = 0;
        size_t i = 0;

        // pieces
        for (; i < fen.length(); i++)
        {
            char c = fen[i];
            // std::cout << fen[i];
            if (c == ' ')
            {
                i++;
                break;
            }
            else if (c == '/')
            {
                rank--;
                file = 0;
            }
            else if (isdigit(c))
                file += c - '0';
            else
            {
                int index = rank * 8 + file;

                switch (c)
                {
                case 'P':
                    set_bit(bitboard[WHITE_PAWN], index);
                    piece_on_square[index] = WHITE_PAWN;
                    occupancy[0] |= 1ULL << index;
                    break;
                case 'N':
                    set_bit(bitboard[WHITE_KNIGHT], index);
                    piece_on_square[index] = WHITE_KNIGHT;
                    occupancy[0] |= 1ULL << index;
                    break;
                case 'B':
                    set_bit(bitboard[WHITE_BISHOP], index);
                    piece_on_square[index] = WHITE_BISHOP;
                    occupancy[0] |= 1ULL << index;
                    break;
                case 'R':
                    set_bit(bitboard[WHITE_ROOK], index);
                    piece_on_square[index] = WHITE_ROOK;
                    occupancy[0] |= 1ULL << index;
                    break;
                case 'Q':
                    set_bit(bitboard[WHITE_QUEEN], index);
                    piece_on_square[index] = WHITE_QUEEN;
                    occupancy[0] |= 1ULL << index;
                    break;
                case 'K':
                    set_bit(bitboard[WHITE_KING], index);
                    piece_on_square[index] = WHITE_KING;
                    occupancy[0] |= 1ULL << index;
                    break;
                case 'p':
                    set_bit(bitboard[BLACK_PAWN], index);
                    piece_on_square[index] = BLACK_PAWN;
                    occupancy[1] |= 1ULL << index;
                    break;
                case 'n':
                    set_bit(bitboard[BLACK_KNIGHT], index);
                    piece_on_square[index] = BLACK_KNIGHT;
                    occupancy[1] |= 1ULL << index;
                    break;
                case 'b':
                    set_bit(bitboard[BLACK_BISHOP], index);
                    piece_on_square[index] = BLACK_BISHOP;
                    occupancy[1] |= 1ULL << index;
                    break;
                case 'r':
                    set_bit(bitboard[BLACK_ROOK], index);
                    piece_on_square[index] = BLACK_ROOK;
                    occupancy[1] |= 1ULL << index;
                    break;
                case 'q':
                    set_bit(bitboard[BLACK_QUEEN], index);
                    piece_on_square[index] = BLACK_QUEEN;
                    occupancy[1] |= 1ULL << index;
                    break;
                case 'k':
                    set_bit(bitboard[BLACK_KING], index);
                    piece_on_square[index] = BLACK_KING;
                    occupancy[1] |= 1ULL << index;
                    break;
                }
                file++;
            }
        }
        occupancy[2] = occupancy[0] | occupancy[1];

        // side to move
        if (i < fen.length())
        {
            // std::cout << fen[i];
            side_to_move = (fen[i] == 'w') ? 0 : 1;
            i += 2;
        }

        // castling rights
        castling_right = 0;
        if (i < fen.length() && fen[i] != '-')
        {
            while (i < fen.length() && fen[i] != ' ')
            {
                // std::cout << fen[i];
                switch (fen[i])
                {
                case 'K':
                    castling_right |= 8;
                    break;
                case 'Q':
                    castling_right |= 4;
                    break;
                case 'k':
                    castling_right |= 2;
                    break;
                case 'q':
                    castling_right |= 1;
                    break;
                }
                i++;
            }
        }
        else if (i < fen.length() && fen[i] == '-')
        {
            // std::cout << fen[i];
            castling_right = 0;
            i++;
        }
        i++;

        // ep square
        ep_square = -1;
        if (i < fen.length() && fen[i] != '-')
        {
            // std::cout << fen[i];
            int f = fen[i++] - 'a';
            int r = fen[i++] - '1';
            ep_square = r * 8 + f;
        }
        else if (i < fen.length() && fen[i] == '-')
        {
            // std::cout << fen[i];
            ep_square = -1;
            i++;
        }
        i++;

        // half move clock
        half_move_clock = 0;
        if (i < fen.length())
        {
            std::string s = "";
            while (i < fen.length() && fen[i] != ' ')
            {
                // std::cout << fen[i];
                s.append(1, fen[i++]);
            }
            try
            {
                half_move_clock = std::stoi(s);
            }
            catch (const std::invalid_argument &e)
            {
                std::cout << s;
                std::cout << " That wasn't a valid number!" << std::endl;
            }
        }
        i++;

        // full move number
        full_move_number = 1;
        if (i < fen.length())
        {
            std::string s = "";
            while (i < fen.length() && fen[i] != ' ')
            {
                // std::cout << fen[i];
                s.append(1, fen[i++]);
            }
            // std::cout << std::endl;
            try
            {
                full_move_number = std::stoi(s);
            }
            catch (const std::invalid_argument &e)
            {
                std::cout << s;
                std::cout << " That wasn't a valid number!" << std::endl;
            }
        }

        // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq b6 0 10
    }
    void init_eval()
    {

        for (int sq = 0; sq < 64; sq++)
        {
            int p = piece_on_square[sq];
            if (p == -1)
                continue;
            game_phase += phase_piece_value[piece_on_square[sq]];
            mg_eval += mg_table[piece_on_square[sq]][sq];
            eg_eval += eg_table[piece_on_square[sq]][sq];
        }
    }
    // checks if the passed square is attacked by any of the enemy pieces
    bool is_square_attacked(int square, int enemy_colour)
    {
        if (enemy_colour) // black attacks
            return (bitboard[BLACK_PAWN] & white_pawn_attacks[square]) | (bitboard[BLACK_KNIGHT] & knight_attacks[square]) | ((bitboard[BLACK_BISHOP] | bitboard[BLACK_QUEEN]) & get_bishop_attacks(square, occupancy[2])) | ((bitboard[BLACK_ROOK] | bitboard[BLACK_QUEEN]) & get_rook_attacks(square, occupancy[2])) | (bitboard[11] & king_attacks[square]);
        else // white attacks
            return (bitboard[WHITE_PAWN] & black_pawn_attacks[square]) | (bitboard[WHITE_KNIGHT] & knight_attacks[square]) | ((bitboard[WHITE_BISHOP] | bitboard[WHITE_QUEEN]) & get_bishop_attacks(square, occupancy[2])) | ((bitboard[WHITE_ROOK] | bitboard[WHITE_QUEEN]) & get_rook_attacks(square, occupancy[2])) | (bitboard[5] & king_attacks[square]);
    }
    // generate a mask of all pseduo legal moves for rookf
    uint64_t get_rook_attacks(int square, uint64_t occupancy)
    {
        uint64_t blocker = occupancy & rook_mask[square];
        int magic_index = (blocker * rook_magic_number[square]) >> (64 - __builtin_popcountll(rook_mask[square]));
        return rook_attacks[square][magic_index];
    }
    // generate a mask of all pseduo legal moves for bishop
    uint64_t get_bishop_attacks(int square, uint64_t occupancy)
    {
        uint64_t blocker = occupancy & bishop_mask[square];
        int magic_index = (blocker * bishop_magic_number[square]) >> (64 - __builtin_popcountll(bishop_mask[square]));
        return bishop_attacks[square][magic_index];
    }
    // generate a mask of all pseduo legal moves for queen
    uint64_t get_queen_attacks(int square, uint64_t occupancy)
    {
        return get_bishop_attacks(square, occupancy) | get_rook_attacks(square, occupancy);
    }
    // encode a move into 16 bits flags::to_square::from_square
    uint16_t encode_move(uint8_t flags, uint8_t from_square, uint8_t to_square)
    {
        return flags << 12 | to_square << 6 | from_square;
    }
    //
    uint64_t generate_hash()
    {
        uint64_t hash = 0;
        for (int piece = 0; piece < 12; piece++)
        {
            uint64_t board = bitboard[piece];
            while (board)
            {
                int sq = __builtin_ctzll(board);
                hash ^= piece_keys[piece][sq];
                board &= board - 1;
            }
        }
        if (ep_square != -1)
        {
            hash ^= enpassant_keys[ep_square];
        }
        hash ^= castle_keys[castling_right];
        if (side_to_move)
            hash ^= side_key;

        return hash;
    }

    // 0-5: white, 6-11: black
    // 0: pawn, 1: knight, 2: bishop, 3: rook,  4: queen,  5: king
    // 6: pawn, 7: knight, 8: bishop, 9: rook, 10: queen, 11: king
    uint64_t bitboard[12];

    // 0: white, 1: black, 2:both
    uint64_t occupancy[3];

    // for instant look up
    int piece_on_square[64];

    // for unmaking the move
    UnmakeInfo history[512];

    // 0: white, 1: black
    int side_to_move = 0;

    // WKS, WQS, BKS, BQS
    int castling_right = 0;

    // destination square of the capturing peice
    int ep_square = -1;

    // count number of half move made(resets at pawn move or any capture)
    int half_move_clock = 0;

    // represent the current half move number
    int half_move_number = 1;

    // represent the current move number
    int full_move_number = 10;

    // int move_count = 0;

    uint64_t current_hash;

    int game_phase = 0;
    int mg_eval = 0;
    int eg_eval = 0;

    uint16_t killer_moves[64][2] = {{0}};

    int history_table[12][64] = {{0}};

    GameState()
    {
        std::fill(bitboard, bitboard + 12, 0ULL);
        std::fill(piece_on_square, piece_on_square + 64, -1);
        std::fill(occupancy, occupancy + 3, 0);
        side_to_move = 0;
        castling_right = 0;
        ep_square = -1;
        half_move_clock = 0;
        half_move_number = 1;
        full_move_number = 1;
        game_phase = 0;
        mg_eval = 0;
        eg_eval = 0;
        current_hash = generate_hash();
    }

    void reset_board()
    {
        std::fill(bitboard, bitboard + 12, 0ULL);
        std::fill(piece_on_square, piece_on_square + 64, -1);
        std::fill(occupancy, occupancy + 3, 0);
        side_to_move = 0;
        castling_right = 0;
        ep_square = -1;
        half_move_clock = 0;
        half_move_number = 1;
        full_move_number = 1;
        game_phase = 0;
        mg_eval = 0;
        eg_eval = 0;
        current_hash = generate_hash();
        for (int i = 0; i < 64; i++)
        {
            killer_moves[i][0] = 0;
            killer_moves[i][1] = 0;
        }
        for (int i = 0; i < 12; i++)
        {
            for (int j = 0; j < 64; j++)
            {
                history_table[i][j] = 0;
            }
        }
    }

    GameState(std::string fen)
    {
        std::fill(bitboard, bitboard + 12, 0ULL);
        std::fill(piece_on_square, piece_on_square + 64, -1);
        std::fill(occupancy, occupancy + 3, 0);
        side_to_move = 0;
        castling_right = 0;
        ep_square = -1;
        half_move_clock = 0;
        full_move_number = 1;
        fen_parser(fen);
        init_eval();
        current_hash = generate_hash();
    }

    void score_move(Move &move, int depth)
    {
        uint8_t from_square = move.move & 0x3f;
        uint8_t to_square = (move.move >> 6) & 0x3f;
        uint8_t flag = (move.move >> 12) & 0x0f;

        if (flag == STANDARD_CAPTURE || (flag >= KNIGHT_PROMOTION_AND_CAPTURE && flag <= QUEEN_PROMOTION_AND_CAPTURE))
        {
            move.score = 1000000 + piece_values[piece_on_square[to_square]] - piece_values[piece_on_square[from_square]];
        }
        else if (flag == EN_PASSANT)
        {
            move.score = 1000000;
        }
        else if (flag == KINGSIDE_CASTLE || flag == QUEENSIDE_CASTLE || (flag >= KNIGHT_PROMOTION && flag <= QUEEN_PROMOTION))
        {
            move.score = 500000;
        }
        else
        {
            if (move.move == killer_moves[depth][0])
            {
                move.score = 900000; // Primary killer move sorts right below captures
            }
            else if (move.move == killer_moves[depth][1])
            {
                move.score = 800000; // Secondary killer move
            }
            else
            {
                move.score = history_table[piece_on_square[from_square]][to_square]; // All other quiet moves -> history heuristic tiebreaker
            }
        }
    }

    void generate_move(MoveList &list, int depth, uint16_t skip_move = 0)
    {
        // Track where new moves begin so we don't overwrite the TT move's 200000 score
        int start_index = list.move_count;

        // Helper lambda to cleanly check for the skip_move before adding
        auto add_move = [&](uint16_t m)
        {
            if (m != skip_move)
            {
                list.moves_list[list.move_count++].move = m;
            }
        };

        if (side_to_move)
        {
            // knights
            uint64_t knights = bitboard[BLACK_KNIGHT];
            while (knights)
            {
                int sq = __builtin_ctzll(knights);
                uint64_t attacks = knight_attacks[sq] & ~occupancy[1];
                uint64_t captures = attacks & occupancy[0];
                attacks ^= captures;
                while (attacks)
                {
                    int attack_sq = __builtin_ctzll(attacks);
                    add_move(encode_move(STANDARD_QUIET_MOVE, sq, attack_sq));
                    attacks &= attacks - 1;
                }
                while (captures)
                {
                    int capture_sq = __builtin_ctzll(captures);
                    add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                    captures &= captures - 1;
                }
                knights &= knights - 1;
            }

            // kings
            int sq = __builtin_ctzll(bitboard[BLACK_KING]);
            uint64_t attacks = king_attacks[sq] & ~occupancy[1];
            uint64_t captures = attacks & occupancy[0];
            attacks ^= captures;
            while (attacks)
            {
                int attack_sq = __builtin_ctzll(attacks);
                add_move(encode_move(STANDARD_QUIET_MOVE, sq, attack_sq));
                attacks &= attacks - 1;
            }
            while (captures)
            {
                int capture_sq = __builtin_ctzll(captures);
                add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                captures &= captures - 1;
            }

            // bishops
            uint64_t bishops = bitboard[BLACK_BISHOP];
            while (bishops)
            {
                int sq = __builtin_ctzll(bishops);
                uint64_t attacks = get_bishop_attacks(sq, occupancy[2]) & ~occupancy[1];
                uint64_t captures = attacks & occupancy[0];
                attacks ^= captures;
                while (attacks)
                {
                    int attack_sq = __builtin_ctzll(attacks);
                    add_move(encode_move(STANDARD_QUIET_MOVE, sq, attack_sq));
                    attacks &= attacks - 1;
                }
                while (captures)
                {
                    int capture_sq = __builtin_ctzll(captures);
                    add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                    captures &= captures - 1;
                }
                bishops &= bishops - 1;
            }

            // rooks
            uint64_t rooks = bitboard[BLACK_ROOK];
            while (rooks)
            {
                int sq = __builtin_ctzll(rooks);
                uint64_t attacks = get_rook_attacks(sq, occupancy[2]) & ~occupancy[1];
                uint64_t captures = attacks & occupancy[0];
                attacks ^= captures;
                while (attacks)
                {
                    int attack_sq = __builtin_ctzll(attacks);
                    add_move(encode_move(STANDARD_QUIET_MOVE, sq, attack_sq));
                    attacks &= attacks - 1;
                }
                while (captures)
                {
                    int capture_sq = __builtin_ctzll(captures);
                    add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                    captures &= captures - 1;
                }
                rooks &= rooks - 1;
            }

            // queens
            uint64_t queens = bitboard[BLACK_QUEEN];
            while (queens)
            {
                int sq = __builtin_ctzll(queens);
                uint64_t attacks = get_queen_attacks(sq, occupancy[2]) & ~occupancy[1];
                uint64_t captures = attacks & occupancy[0];
                attacks ^= captures;
                while (attacks)
                {
                    int attack_sq = __builtin_ctzll(attacks);
                    add_move(encode_move(STANDARD_QUIET_MOVE, sq, attack_sq));
                    attacks &= attacks - 1;
                }
                while (captures)
                {
                    int capture_sq = __builtin_ctzll(captures);
                    add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                    captures &= captures - 1;
                }
                queens &= queens - 1;
            }

            // pawns
            uint64_t pawns = bitboard[BLACK_PAWN] & ~RANK_2_MASK;
            uint64_t single_push = (pawns >> 8) & ~occupancy[2];
            uint64_t double_push = ((single_push >> 8) & RANK_5_MASK) & ~occupancy[2];
            while (pawns)
            {
                int sq = __builtin_ctzll(pawns);
                uint64_t captures = black_pawn_attacks[sq] & occupancy[0];
                while (captures)
                {
                    int capture_sq = __builtin_ctzll(captures);
                    add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                    captures &= captures - 1;
                }
                pawns &= pawns - 1;
            }
            while (single_push)
            {
                int sq = __builtin_ctzll(single_push);
                add_move(encode_move(STANDARD_QUIET_MOVE, sq + 8, sq));
                single_push &= single_push - 1;
            }
            while (double_push)
            {
                int sq = __builtin_ctzll(double_push);
                add_move(encode_move(STANDARD_QUIET_MOVE, sq + 16, sq));
                double_push &= double_push - 1;
            }

            // castle
            if ((castling_right & 1 << 1) && !(occupancy[2] & 0x6000000000000000ULL) && !is_square_attacked(60, 0) && !is_square_attacked(61, 0) && !is_square_attacked(62, 0))
                add_move(encode_move(KINGSIDE_CASTLE, 60, 62));
            if ((castling_right & 1 << 0) && !(occupancy[2] & 0x0E00000000000000ULL) && !is_square_attacked(58, 0) && !is_square_attacked(59, 0) && !is_square_attacked(60, 0))
                add_move(encode_move(QUEENSIDE_CASTLE, 60, 58));

            // promotion
            uint64_t promotion_pawns = bitboard[BLACK_PAWN] & RANK_2_MASK;
            uint64_t promotion_dr_captures = ((promotion_pawns & NOT_H_FILE) >> 7) & occupancy[0];
            uint64_t promotion_dl_captures = ((promotion_pawns & NOT_A_FILE) >> 9) & occupancy[0];
            uint64_t promotion_d_captures = (promotion_pawns >> 8) & ~occupancy[2];

            while (promotion_dr_captures)
            {
                int sq = __builtin_ctzll(promotion_dr_captures);
                add_move(encode_move(KNIGHT_PROMOTION_AND_CAPTURE, sq + 7, sq));
                add_move(encode_move(BISHOP_PROMOTION_AND_CAPTURE, sq + 7, sq));
                add_move(encode_move(ROOK_PROMOTION_AND_CAPTURE, sq + 7, sq));
                add_move(encode_move(QUEEN_PROMOTION_AND_CAPTURE, sq + 7, sq));
                promotion_dr_captures &= promotion_dr_captures - 1;
            }
            while (promotion_dl_captures)
            {
                int sq = __builtin_ctzll(promotion_dl_captures);
                add_move(encode_move(KNIGHT_PROMOTION_AND_CAPTURE, sq + 9, sq));
                add_move(encode_move(BISHOP_PROMOTION_AND_CAPTURE, sq + 9, sq));
                add_move(encode_move(ROOK_PROMOTION_AND_CAPTURE, sq + 9, sq));
                add_move(encode_move(QUEEN_PROMOTION_AND_CAPTURE, sq + 9, sq));
                promotion_dl_captures &= promotion_dl_captures - 1;
            }
            while (promotion_d_captures)
            {
                int sq = __builtin_ctzll(promotion_d_captures);
                add_move(encode_move(KNIGHT_PROMOTION, sq + 8, sq));
                add_move(encode_move(BISHOP_PROMOTION, sq + 8, sq));
                add_move(encode_move(ROOK_PROMOTION, sq + 8, sq));
                add_move(encode_move(QUEEN_PROMOTION, sq + 8, sq));
                promotion_d_captures &= promotion_d_captures - 1;
            }

            // en passant
            if (ep_square >= 0)
            {
                uint64_t ep_attackers = white_pawn_attacks[ep_square] & bitboard[BLACK_PAWN];
                while (ep_attackers)
                {
                    add_move(encode_move(EN_PASSANT, __builtin_ctzll(ep_attackers), ep_square));
                    ep_attackers &= ep_attackers - 1;
                }
            }
        }
        else
        {
            // knights
            uint64_t knights = bitboard[WHITE_KNIGHT];
            while (knights)
            {
                int sq = __builtin_ctzll(knights);
                uint64_t attacks = knight_attacks[sq] & ~occupancy[0];
                uint64_t captures = attacks & occupancy[1];
                attacks ^= captures;
                while (attacks)
                {
                    int attack_sq = __builtin_ctzll(attacks);
                    add_move(encode_move(STANDARD_QUIET_MOVE, sq, attack_sq));
                    attacks &= attacks - 1;
                }
                while (captures)
                {
                    int capture_sq = __builtin_ctzll(captures);
                    add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                    captures &= captures - 1;
                }
                knights &= knights - 1;
            }

            // kings
            int sq = __builtin_ctzll(bitboard[WHITE_KING]);
            uint64_t attacks = king_attacks[sq] & ~occupancy[0];
            uint64_t captures = attacks & occupancy[1];
            attacks ^= captures;
            while (attacks)
            {
                int attack_sq = __builtin_ctzll(attacks);
                add_move(encode_move(STANDARD_QUIET_MOVE, sq, attack_sq));
                attacks &= attacks - 1;
            }
            while (captures)
            {
                int capture_sq = __builtin_ctzll(captures);
                add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                captures &= captures - 1;
            }

            // bishops
            uint64_t bishops = bitboard[WHITE_BISHOP];
            while (bishops)
            {
                int sq = __builtin_ctzll(bishops);
                uint64_t attacks = get_bishop_attacks(sq, occupancy[2]) & ~occupancy[0];
                uint64_t captures = attacks & occupancy[1];
                attacks ^= captures;
                while (attacks)
                {
                    int attack_sq = __builtin_ctzll(attacks);
                    add_move(encode_move(STANDARD_QUIET_MOVE, sq, attack_sq));
                    attacks &= attacks - 1;
                }
                while (captures)
                {
                    int capture_sq = __builtin_ctzll(captures);
                    add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                    captures &= captures - 1;
                }
                bishops &= bishops - 1;
            }

            // rooks
            uint64_t rooks = bitboard[WHITE_ROOK];
            while (rooks)
            {
                int sq = __builtin_ctzll(rooks);
                uint64_t attacks = get_rook_attacks(sq, occupancy[2]) & ~occupancy[0];
                uint64_t captures = attacks & occupancy[1];
                attacks ^= captures;
                while (attacks)
                {
                    int attack_sq = __builtin_ctzll(attacks);
                    add_move(encode_move(STANDARD_QUIET_MOVE, sq, attack_sq));
                    attacks &= attacks - 1;
                }
                while (captures)
                {
                    int capture_sq = __builtin_ctzll(captures);
                    add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                    captures &= captures - 1;
                }
                rooks &= rooks - 1;
            }

            // queens
            uint64_t queens = bitboard[WHITE_QUEEN];
            while (queens)
            {
                int sq = __builtin_ctzll(queens);
                uint64_t attacks = get_queen_attacks(sq, occupancy[2]) & ~occupancy[0];
                uint64_t captures = attacks & occupancy[1];
                attacks ^= captures;
                while (attacks)
                {
                    int attack_sq = __builtin_ctzll(attacks);
                    add_move(encode_move(STANDARD_QUIET_MOVE, sq, attack_sq));
                    attacks &= attacks - 1;
                }
                while (captures)
                {
                    int capture_sq = __builtin_ctzll(captures);
                    add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                    captures &= captures - 1;
                }
                queens &= queens - 1;
            }

            // pawns
            uint64_t pawns = bitboard[WHITE_PAWN] & ~RANK_7_MASK;
            uint64_t single_push = (pawns << 8) & ~occupancy[2];
            uint64_t double_push = ((single_push << 8) & RANK_4_MASK) & ~occupancy[2];
            while (pawns)
            {
                int sq = __builtin_ctzll(pawns);
                uint64_t captures = white_pawn_attacks[sq] & occupancy[1];
                while (captures)
                {
                    int capture_sq = __builtin_ctzll(captures);
                    add_move(encode_move(STANDARD_CAPTURE, sq, capture_sq));
                    captures &= captures - 1;
                }
                pawns &= pawns - 1;
            }
            while (single_push)
            {
                int sq = __builtin_ctzll(single_push);
                add_move(encode_move(STANDARD_QUIET_MOVE, sq - 8, sq));
                single_push &= single_push - 1;
            }
            while (double_push)
            {
                int sq = __builtin_ctzll(double_push);
                add_move(encode_move(STANDARD_QUIET_MOVE, sq - 16, sq));
                double_push &= double_push - 1;
            }

            // castling
            if ((castling_right & 1 << 3) && !(occupancy[2] & 0x60ULL) && !is_square_attacked(4, 1) && !is_square_attacked(5, 1) && !is_square_attacked(6, 1))
                add_move(encode_move(KINGSIDE_CASTLE, 4, 6));
            if ((castling_right & 1 << 2) && !(occupancy[2] & 0x0EULL) && !is_square_attacked(2, 1) && !is_square_attacked(3, 1) && !is_square_attacked(4, 1))
                add_move(encode_move(QUEENSIDE_CASTLE, 4, 2));

            // promotion
            uint64_t promotion_pawns = bitboard[WHITE_PAWN] & RANK_7_MASK;
            uint64_t promotion_ur_captures = ((promotion_pawns & NOT_H_FILE) << 9) & occupancy[1];
            uint64_t promotion_ul_captures = ((promotion_pawns & NOT_A_FILE) << 7) & occupancy[1];
            uint64_t promotion_u_captures = (promotion_pawns << 8) & ~occupancy[2];

            while (promotion_ur_captures)
            {
                int sq = __builtin_ctzll(promotion_ur_captures);
                add_move(encode_move(KNIGHT_PROMOTION_AND_CAPTURE, sq - 9, sq));
                add_move(encode_move(BISHOP_PROMOTION_AND_CAPTURE, sq - 9, sq));
                add_move(encode_move(ROOK_PROMOTION_AND_CAPTURE, sq - 9, sq));
                add_move(encode_move(QUEEN_PROMOTION_AND_CAPTURE, sq - 9, sq));
                promotion_ur_captures &= promotion_ur_captures - 1;
            }
            while (promotion_ul_captures)
            {
                int sq = __builtin_ctzll(promotion_ul_captures);
                add_move(encode_move(KNIGHT_PROMOTION_AND_CAPTURE, sq - 7, sq));
                add_move(encode_move(BISHOP_PROMOTION_AND_CAPTURE, sq - 7, sq));
                add_move(encode_move(ROOK_PROMOTION_AND_CAPTURE, sq - 7, sq));
                add_move(encode_move(QUEEN_PROMOTION_AND_CAPTURE, sq - 7, sq));
                promotion_ul_captures &= promotion_ul_captures - 1;
            }
            while (promotion_u_captures)
            {
                int sq = __builtin_ctzll(promotion_u_captures);
                add_move(encode_move(KNIGHT_PROMOTION, sq - 8, sq));
                add_move(encode_move(BISHOP_PROMOTION, sq - 8, sq));
                add_move(encode_move(ROOK_PROMOTION, sq - 8, sq));
                add_move(encode_move(QUEEN_PROMOTION, sq - 8, sq));
                promotion_u_captures &= promotion_u_captures - 1;
            }

            // en passant
            if (ep_square >= 0)
            {
                uint64_t ep_attackers = black_pawn_attacks[ep_square] & bitboard[WHITE_PAWN];
                while (ep_attackers)
                {
                    add_move(encode_move(EN_PASSANT, __builtin_ctzll(ep_attackers), ep_square));
                    ep_attackers &= ep_attackers - 1;
                }
            }
        }

        // Only score the newly generated moves (preserves the TT move's 200000 score at index 0)
        for (int i = start_index; i < list.move_count; i++)
        {
            score_move(list.moves_list[i], depth);
        }
    }

    void make_move(uint16_t move)
    {
        // extract variable from encoded move
        uint8_t from_square = move & 0x3f;
        uint8_t to_square = (move >> 6) & 0x3f;
        uint8_t flag = (move >> 12) & 0x0f;

        // update the hash

        // save the state variables in history
        history[half_move_number].castling_right = castling_right;
        history[half_move_number].ep_square = ep_square;
        history[half_move_number].captured_piece = piece_on_square[to_square];
        history[half_move_number].mg_eval = mg_eval;
        history[half_move_number].eg_eval = eg_eval;
        history[half_move_number].game_phase = game_phase;
        history[half_move_number].hash = current_hash;

        // update the castling rights
        current_hash ^= castle_keys[castling_right];
        castling_right &= castling_rights_update[from_square];
        castling_right &= castling_rights_update[to_square];
        current_hash ^= castle_keys[castling_right];

        int from_piece = piece_on_square[from_square];
        int to_piece = piece_on_square[to_square];

        // update from piece eval
        mg_eval -= mg_table[from_piece][from_square];
        eg_eval -= eg_table[from_piece][from_square];
        mg_eval += mg_table[from_piece][to_square];
        eg_eval += eg_table[from_piece][to_square];

        // remove the from_piece from the from_square(source)
        current_hash ^= piece_keys[piece_on_square[from_square]][from_square];

        bitboard[from_piece] ^= 1ULL << from_square;
        piece_on_square[from_square] = -1;

        if (from_piece <= 5)
        {
            occupancy[0] ^= 1ULL << from_square;
        }
        else
        {
            occupancy[1] ^= 1ULL << from_square;
        }

        // make the captures if any
        bool isCapture = (flag == STANDARD_CAPTURE || (flag >= KNIGHT_PROMOTION_AND_CAPTURE && flag <= QUEEN_PROMOTION_AND_CAPTURE));
        if (isCapture)
        {
            // update captured(to) piece eval
            mg_eval -= mg_table[to_piece][to_square];
            eg_eval -= eg_table[to_piece][to_square];
            game_phase -= phase_piece_value[to_piece];

            current_hash ^= piece_keys[piece_on_square[to_square]][to_square];

            bitboard[to_piece] ^= 1ULL << to_square;

            if (from_piece <= 5)
            {
                occupancy[1] ^= 1ULL << to_square;
            }
            else
            {
                occupancy[0] ^= 1ULL << to_square;
            }
        }

        // put the from_piece to the to_square(destination)
        current_hash ^= piece_keys[from_piece][to_square];

        bitboard[from_piece] ^= 1ULL << to_square;
        piece_on_square[to_square] = from_piece;

        if (from_piece <= 5)
        {
            occupancy[0] ^= 1ULL << to_square;
        }
        else
        {
            occupancy[1] ^= 1ULL << to_square;
        }

        // handle the special moves if any
        switch (flag)
        {
        case KNIGHT_PROMOTION_AND_CAPTURE:
        case KNIGHT_PROMOTION:
        {
            if (from_piece == WHITE_PAWN)
            {
                // update eval
                mg_eval -= mg_table[WHITE_PAWN][to_square];
                eg_eval -= eg_table[WHITE_PAWN][to_square];
                mg_eval += mg_table[WHITE_KNIGHT][to_square];
                eg_eval += eg_table[WHITE_KNIGHT][to_square];

                game_phase += phase_piece_value[WHITE_KNIGHT];

                current_hash ^= piece_keys[WHITE_PAWN][to_square];
                current_hash ^= piece_keys[WHITE_KNIGHT][to_square];

                bitboard[WHITE_PAWN] ^= 1ULL << to_square;
                bitboard[WHITE_KNIGHT] ^= 1ULL << to_square;
                piece_on_square[to_square] = WHITE_KNIGHT;
            }
            else
            {
                // update eval
                mg_eval -= mg_table[BLACK_PAWN][to_square];
                eg_eval -= eg_table[BLACK_PAWN][to_square];
                mg_eval += mg_table[BLACK_KNIGHT][to_square];
                eg_eval += eg_table[BLACK_KNIGHT][to_square];

                game_phase += phase_piece_value[BLACK_KNIGHT];

                current_hash ^= piece_keys[BLACK_PAWN][to_square];
                current_hash ^= piece_keys[BLACK_KNIGHT][to_square];

                bitboard[BLACK_PAWN] ^= 1ULL << to_square;
                bitboard[BLACK_KNIGHT] ^= 1ULL << to_square;
                piece_on_square[to_square] = BLACK_KNIGHT;
            }
            break;
        }
        case BISHOP_PROMOTION_AND_CAPTURE:
        case BISHOP_PROMOTION:
        {
            if (from_piece == WHITE_PAWN)
            {
                // update eval
                mg_eval -= mg_table[WHITE_PAWN][to_square];
                eg_eval -= eg_table[WHITE_PAWN][to_square];
                mg_eval += mg_table[WHITE_BISHOP][to_square];
                eg_eval += eg_table[WHITE_BISHOP][to_square];

                game_phase += phase_piece_value[WHITE_BISHOP];

                current_hash ^= piece_keys[WHITE_PAWN][to_square];
                current_hash ^= piece_keys[WHITE_BISHOP][to_square];

                bitboard[WHITE_PAWN] ^= 1ULL << to_square;
                bitboard[WHITE_BISHOP] ^= 1ULL << to_square;
                piece_on_square[to_square] = WHITE_BISHOP;
            }
            else
            {
                // update eval
                mg_eval -= mg_table[BLACK_PAWN][to_square];
                eg_eval -= eg_table[BLACK_PAWN][to_square];
                mg_eval += mg_table[BLACK_BISHOP][to_square];
                eg_eval += eg_table[BLACK_BISHOP][to_square];

                game_phase += phase_piece_value[BLACK_BISHOP];

                current_hash ^= piece_keys[BLACK_PAWN][to_square];
                current_hash ^= piece_keys[BLACK_BISHOP][to_square];

                bitboard[BLACK_PAWN] ^= 1ULL << to_square;
                bitboard[BLACK_BISHOP] ^= 1ULL << to_square;
                piece_on_square[to_square] = BLACK_BISHOP;
            }
            break;
        }
        case ROOK_PROMOTION_AND_CAPTURE:
        case ROOK_PROMOTION:
        {
            if (from_piece == WHITE_PAWN)
            {
                // update eval
                mg_eval -= mg_table[WHITE_PAWN][to_square];
                eg_eval -= eg_table[WHITE_PAWN][to_square];
                mg_eval += mg_table[WHITE_ROOK][to_square];
                eg_eval += eg_table[WHITE_ROOK][to_square];

                game_phase += phase_piece_value[WHITE_ROOK];

                current_hash ^= piece_keys[WHITE_PAWN][to_square];
                current_hash ^= piece_keys[WHITE_ROOK][to_square];

                bitboard[WHITE_PAWN] ^= 1ULL << to_square;
                bitboard[WHITE_ROOK] ^= 1ULL << to_square;
                piece_on_square[to_square] = WHITE_ROOK;
            }
            else
            {
                // update eval
                mg_eval -= mg_table[BLACK_PAWN][to_square];
                eg_eval -= eg_table[BLACK_PAWN][to_square];
                mg_eval += mg_table[BLACK_ROOK][to_square];
                eg_eval += eg_table[BLACK_ROOK][to_square];

                game_phase += phase_piece_value[BLACK_ROOK];

                current_hash ^= piece_keys[BLACK_PAWN][to_square];
                current_hash ^= piece_keys[BLACK_ROOK][to_square];

                bitboard[BLACK_PAWN] ^= 1ULL << to_square;
                bitboard[BLACK_ROOK] ^= 1ULL << to_square;
                piece_on_square[to_square] = BLACK_ROOK;
            }
            break;
        }
        case QUEEN_PROMOTION_AND_CAPTURE:
        case QUEEN_PROMOTION:
        {
            if (from_piece == WHITE_PAWN)
            {
                // update eval
                mg_eval -= mg_table[WHITE_PAWN][to_square];
                eg_eval -= eg_table[WHITE_PAWN][to_square];
                mg_eval += mg_table[WHITE_QUEEN][to_square];
                eg_eval += eg_table[WHITE_QUEEN][to_square];

                game_phase += phase_piece_value[WHITE_QUEEN];

                current_hash ^= piece_keys[WHITE_PAWN][to_square];
                current_hash ^= piece_keys[WHITE_QUEEN][to_square];

                bitboard[WHITE_PAWN] ^= 1ULL << to_square;
                bitboard[WHITE_QUEEN] ^= 1ULL << to_square;
                piece_on_square[to_square] = WHITE_QUEEN;
            }
            else
            {
                // update eval
                mg_eval -= mg_table[BLACK_PAWN][to_square];
                eg_eval -= eg_table[BLACK_PAWN][to_square];
                mg_eval += mg_table[BLACK_QUEEN][to_square];
                eg_eval += eg_table[BLACK_QUEEN][to_square];

                game_phase += phase_piece_value[BLACK_QUEEN];

                current_hash ^= piece_keys[BLACK_PAWN][to_square];
                current_hash ^= piece_keys[BLACK_QUEEN][to_square];

                bitboard[BLACK_PAWN] ^= 1ULL << to_square;
                bitboard[BLACK_QUEEN] ^= 1ULL << to_square;
                piece_on_square[to_square] = BLACK_QUEEN;
            }
            break;
        }
        case KINGSIDE_CASTLE:
        {
            if (from_piece == WHITE_KING)
            {
                // update eval
                mg_eval -= mg_table[WHITE_ROOK][h1];
                eg_eval -= eg_table[WHITE_ROOK][h1];
                mg_eval += mg_table[WHITE_ROOK][f1];
                eg_eval += eg_table[WHITE_ROOK][f1];

                current_hash ^= piece_keys[WHITE_ROOK][5];
                current_hash ^= piece_keys[WHITE_ROOK][7];

                bitboard[WHITE_ROOK] ^= 0xa0ULL;
                piece_on_square[h1] = -1;
                piece_on_square[f1] = WHITE_ROOK;
                occupancy[0] ^= 0xa0ULL;
            }
            else
            {
                // update eval
                mg_eval -= mg_table[BLACK_ROOK][h8];
                eg_eval -= eg_table[BLACK_ROOK][h8];
                mg_eval += mg_table[BLACK_ROOK][f8];
                eg_eval += eg_table[BLACK_ROOK][f8];

                current_hash ^= piece_keys[BLACK_ROOK][61];
                current_hash ^= piece_keys[BLACK_ROOK][63];

                bitboard[BLACK_ROOK] ^= 0xa000000000000000ULL;
                piece_on_square[h8] = -1;
                piece_on_square[f8] = BLACK_ROOK;
                occupancy[1] ^= 0xa000000000000000ULL;
            }
            break;
        }
        case QUEENSIDE_CASTLE:
        {
            if (from_piece == WHITE_KING)
            {
                // update eval
                mg_eval -= mg_table[WHITE_ROOK][a1];
                eg_eval -= eg_table[WHITE_ROOK][a1];
                mg_eval += mg_table[WHITE_ROOK][d1];
                eg_eval += eg_table[WHITE_ROOK][d1];

                current_hash ^= piece_keys[WHITE_ROOK][0];
                current_hash ^= piece_keys[WHITE_ROOK][3];

                bitboard[WHITE_ROOK] ^= 0x09ULL;
                piece_on_square[a1] = -1;
                piece_on_square[d1] = WHITE_ROOK;
                occupancy[0] ^= 0x09ULL;
            }
            else
            {
                // update eval
                mg_eval -= mg_table[BLACK_ROOK][a8];
                eg_eval -= eg_table[BLACK_ROOK][a8];
                mg_eval += mg_table[BLACK_ROOK][d8];
                eg_eval += eg_table[BLACK_ROOK][d8];

                current_hash ^= piece_keys[BLACK_ROOK][56];
                current_hash ^= piece_keys[BLACK_ROOK][59];

                bitboard[BLACK_ROOK] ^= 0x0900000000000000ULL;
                piece_on_square[a8] = -1;
                piece_on_square[d8] = BLACK_ROOK;
                occupancy[1] ^= 0x0900000000000000ULL;
            }
            break;
        }
        case EN_PASSANT:
        {
            if (from_piece == WHITE_PAWN)
            {
                int sq = ep_square - 8;

                // update eval
                mg_eval -= mg_table[BLACK_PAWN][sq];
                eg_eval -= eg_table[BLACK_PAWN][sq];

                current_hash ^= piece_keys[BLACK_PAWN][sq];

                bitboard[BLACK_PAWN] ^= 1ULL << (sq);
                piece_on_square[sq] = -1;
                occupancy[1] ^= 1ULL << (sq);
            }
            else
            {
                int sq = ep_square + 8;

                // update eval
                mg_eval -= mg_table[WHITE_PAWN][sq];
                eg_eval -= eg_table[WHITE_PAWN][sq];

                current_hash ^= piece_keys[WHITE_PAWN][sq];

                bitboard[WHITE_PAWN] ^= 1ULL << (sq);
                piece_on_square[sq] = -1;
                occupancy[0] ^= 1ULL << (sq);
            }
            if (ep_square != -1)
                current_hash ^= enpassant_keys[ep_square];
            ep_square = -1;
            break;
        }
        default:
            break;
        }

        // update the global occupancy
        occupancy[2] = occupancy[0] | occupancy[1];

        if (ep_square != -1)
            current_hash ^= enpassant_keys[ep_square];
        ep_square = -1;

        // update ep_square for next move
        if (from_piece == WHITE_PAWN && (to_square - from_square == 16))
        {
            if (white_pawn_attacks[from_square + 8] & bitboard[BLACK_PAWN])
            {
                current_hash ^= enpassant_keys[from_square + 8];
                ep_square = from_square + 8;
            }
        }
        else if (from_piece == BLACK_PAWN && (from_square - to_square == 16))
        {
            if (black_pawn_attacks[from_square - 8] & bitboard[WHITE_PAWN])
            {
                current_hash ^= enpassant_keys[from_square - 8];
                ep_square = from_square - 8;
            }
        }

        full_move_number += !(half_move_number & 1);
        half_move_number++;
        current_hash ^= side_key;
        side_to_move ^= 1;
    }

    void unmake_move(uint16_t move)
    {
        // extract variable from encoded move
        uint8_t from_square = move & 0x3f;
        uint8_t to_square = (move >> 6) & 0x3f; // 0xfc0
        uint8_t flag = (move >> 12) & 0x0f;     // 0xf000

        // update/restore the state variables
        full_move_number -= half_move_number & 1;
        half_move_number--;

        mg_eval = history[half_move_number].mg_eval;
        eg_eval = history[half_move_number].eg_eval;
        game_phase = history[half_move_number].game_phase;

        current_hash ^= castle_keys[castling_right];
        castling_right = history[half_move_number].castling_right;
        current_hash ^= castle_keys[castling_right];

        if (ep_square != -1)
            current_hash ^= enpassant_keys[ep_square];
        ep_square = history[half_move_number].ep_square;
        if (ep_square != -1)
            current_hash ^= enpassant_keys[ep_square];

        int captured_piece = history[half_move_number].captured_piece;
        int to_piece = piece_on_square[to_square];

        // remove the to_piece from the to_square(destination)
        current_hash ^= piece_keys[piece_on_square[to_square]][to_square];

        bitboard[to_piece] ^= 1ULL << to_square;
        piece_on_square[to_square] = -1;

        if (to_piece <= 5)
        {
            occupancy[0] ^= 1ULL << to_square;
        }
        else
        {
            occupancy[1] ^= 1ULL << to_square;
        }

        // unmake the captures if any
        bool isCapture = (flag == STANDARD_CAPTURE || (flag >= KNIGHT_PROMOTION_AND_CAPTURE && flag <= QUEEN_PROMOTION_AND_CAPTURE));
        if (isCapture)
        {
            current_hash ^= piece_keys[captured_piece][to_square];

            bitboard[captured_piece] ^= 1ULL << to_square;

            if (to_piece <= 5)
            {
                occupancy[1] ^= 1ULL << to_square;
            }
            else
            {
                occupancy[0] ^= 1ULL << to_square;
            }
        }
        piece_on_square[to_square] = captured_piece;

        // put the to_piece to the from_square(source)
        current_hash ^= piece_keys[to_piece][from_square];

        bitboard[to_piece] ^= 1ULL << from_square;
        piece_on_square[from_square] = to_piece;

        if (to_piece <= 5)
        {
            occupancy[0] ^= 1ULL << from_square;
        }
        else
        {
            occupancy[1] ^= 1ULL << from_square;
        }

        // handle the special moves if any
        switch (flag)
        {
        case KNIGHT_PROMOTION_AND_CAPTURE:
        case KNIGHT_PROMOTION:
        {
            if (to_piece == WHITE_KNIGHT)
            {
                current_hash ^= piece_keys[WHITE_PAWN][from_square];
                current_hash ^= piece_keys[WHITE_KNIGHT][from_square];

                bitboard[WHITE_PAWN] ^= 1ULL << from_square;
                bitboard[WHITE_KNIGHT] ^= 1ULL << from_square;
                piece_on_square[from_square] = WHITE_PAWN;
            }
            else
            {
                current_hash ^= piece_keys[BLACK_PAWN][from_square];
                current_hash ^= piece_keys[BLACK_KNIGHT][from_square];

                bitboard[BLACK_PAWN] ^= 1ULL << from_square;
                bitboard[BLACK_KNIGHT] ^= 1ULL << from_square;
                piece_on_square[from_square] = BLACK_PAWN;
            }
            break;
        }
        case BISHOP_PROMOTION_AND_CAPTURE:
        case BISHOP_PROMOTION:
        {
            if (to_piece == WHITE_BISHOP)
            {
                current_hash ^= piece_keys[WHITE_PAWN][from_square];
                current_hash ^= piece_keys[WHITE_BISHOP][from_square];

                bitboard[WHITE_PAWN] ^= 1ULL << from_square;
                bitboard[WHITE_BISHOP] ^= 1ULL << from_square;
                piece_on_square[from_square] = WHITE_PAWN;
            }
            else
            {
                current_hash ^= piece_keys[BLACK_PAWN][from_square];
                current_hash ^= piece_keys[BLACK_BISHOP][from_square];

                bitboard[BLACK_PAWN] ^= 1ULL << from_square;
                bitboard[BLACK_BISHOP] ^= 1ULL << from_square;
                piece_on_square[from_square] = BLACK_PAWN;
            }
            break;
        }
        case ROOK_PROMOTION_AND_CAPTURE:
        case ROOK_PROMOTION:
        {
            if (to_piece == WHITE_ROOK)
            {
                current_hash ^= piece_keys[WHITE_PAWN][from_square];
                current_hash ^= piece_keys[WHITE_ROOK][from_square];

                bitboard[WHITE_PAWN] ^= 1ULL << from_square;
                bitboard[WHITE_ROOK] ^= 1ULL << from_square;
                piece_on_square[from_square] = WHITE_PAWN;
            }
            else
            {
                current_hash ^= piece_keys[BLACK_PAWN][from_square];
                current_hash ^= piece_keys[BLACK_ROOK][from_square];

                bitboard[BLACK_PAWN] ^= 1ULL << from_square;
                bitboard[BLACK_ROOK] ^= 1ULL << from_square;
                piece_on_square[from_square] = BLACK_PAWN;
            }
            break;
        }
        case QUEEN_PROMOTION_AND_CAPTURE:
        case QUEEN_PROMOTION:
        {
            if (to_piece == WHITE_QUEEN)
            {
                current_hash ^= piece_keys[WHITE_PAWN][from_square];
                current_hash ^= piece_keys[WHITE_QUEEN][from_square];

                bitboard[WHITE_PAWN] ^= 1ULL << from_square;
                bitboard[WHITE_QUEEN] ^= 1ULL << from_square;
                piece_on_square[from_square] = WHITE_PAWN;
            }
            else
            {
                current_hash ^= piece_keys[BLACK_PAWN][from_square];
                current_hash ^= piece_keys[BLACK_QUEEN][from_square];

                bitboard[BLACK_PAWN] ^= 1ULL << from_square;
                bitboard[BLACK_QUEEN] ^= 1ULL << from_square;
                piece_on_square[from_square] = BLACK_PAWN;
            }
            break;
        }
        case KINGSIDE_CASTLE:
        {
            if (to_piece == WHITE_KING)
            {
                current_hash ^= piece_keys[WHITE_ROOK][5];
                current_hash ^= piece_keys[WHITE_ROOK][7];

                bitboard[WHITE_ROOK] ^= 0xa0ULL;
                piece_on_square[h1] = WHITE_ROOK;
                piece_on_square[f1] = -1;
                occupancy[0] ^= 0xa0ULL;
            }
            else
            {
                current_hash ^= piece_keys[BLACK_ROOK][61];
                current_hash ^= piece_keys[BLACK_ROOK][63];

                bitboard[BLACK_ROOK] ^= 0xa000000000000000ULL;
                piece_on_square[h8] = BLACK_ROOK;
                piece_on_square[f8] = -1;
                occupancy[1] ^= 0xa000000000000000ULL;
            }
            break;
        }
        case QUEENSIDE_CASTLE:
        {
            if (to_piece == WHITE_KING)
            {
                current_hash ^= piece_keys[WHITE_ROOK][0];
                current_hash ^= piece_keys[WHITE_ROOK][3];

                bitboard[WHITE_ROOK] ^= 0x09ULL;
                piece_on_square[a1] = WHITE_ROOK;
                piece_on_square[d1] = -1;
                occupancy[0] ^= 0x09ULL;
            }
            else
            {
                current_hash ^= piece_keys[BLACK_ROOK][56];
                current_hash ^= piece_keys[BLACK_ROOK][59];

                bitboard[BLACK_ROOK] ^= 0x0900000000000000ULL;
                piece_on_square[a8] = BLACK_ROOK;
                piece_on_square[d8] = -1;
                occupancy[1] ^= 0x0900000000000000ULL;
            }
            break;
        }
        case EN_PASSANT:
        {
            if (to_piece == WHITE_PAWN)
            {
                int sq = ep_square - 8;
                current_hash ^= piece_keys[BLACK_PAWN][ep_square - 8];

                bitboard[BLACK_PAWN] ^= 1ULL << (sq);
                piece_on_square[sq] = BLACK_PAWN;
                occupancy[1] ^= 1ULL << (sq);
            }
            else
            {
                int sq = ep_square + 8;
                current_hash ^= piece_keys[WHITE_PAWN][ep_square + 8];

                bitboard[WHITE_PAWN] ^= 1ULL << (sq);
                piece_on_square[sq] = WHITE_PAWN;
                occupancy[0] ^= 1ULL << (sq);
            }
            break;
        }
        default:
            break;
        }

        // update the global occupancy
        occupancy[2] = occupancy[0] | occupancy[1];
        current_hash ^= side_key;
        side_to_move ^= 1;
    }

    FORCE_INLINE void make_null_move()
    {
        // save the state variables in history
        history[half_move_number].hash = current_hash;
        history[half_move_number].ep_square = ep_square;
        if (ep_square != -1)
            current_hash ^= enpassant_keys[ep_square];
        ep_square = -1;

        full_move_number += !(half_move_number & 1);
        half_move_number++;
        current_hash ^= side_key;
        side_to_move ^= 1;
    }

    FORCE_INLINE void unmake_null_move()
    {
        // update/restore the state variables
        full_move_number -= half_move_number & 1;
        half_move_number--;
        ep_square = history[half_move_number].ep_square;
        if (ep_square != -1)
            current_hash ^= enpassant_keys[ep_square];

        current_hash ^= side_key;
        side_to_move ^= 1;
    }

    FORCE_INLINE bool has_non_pawn_material()
    {
        if (side_to_move)
        {
           return bitboard[BLACK_KNIGHT] || bitboard[BLACK_BISHOP] || bitboard[BLACK_ROOK] || bitboard[BLACK_QUEEN];
        }
        else
        {
            return bitboard[WHITE_KNIGHT] || bitboard[WHITE_BISHOP] || bitboard[WHITE_ROOK] || bitboard[WHITE_QUEEN];
        }
    }
    
    FORCE_INLINE bool is_repetition() {
        for (int i = half_move_number - 2; i >= 1; i -= 2) {
            if (current_hash == history[i].hash) return true;
        }
        return false;
    }

    FORCE_INLINE int evaluation()
    {
        int mg_weight = game_phase;
        int eg_weight = 24 - game_phase;

        int score = (mg_eval * mg_weight + eg_eval * eg_weight) / 24;

        return side_to_move ? -score : score;
    }

    uint16_t parse_uci_move(std::string move_str, int depth)
    {
        int from_file = move_str[0] - 'a';
        int from_rank = move_str[1] - '1';
        uint8_t from_sq = from_rank * 8 + from_file;

        int to_file = move_str[2] - 'a';
        int to_rank = move_str[3] - '1';
        uint8_t to_sq = to_rank * 8 + to_file;

        MoveList legal_moves;
        generate_move(legal_moves, depth);

        for (Move move : legal_moves.moves_list)
        {
            uint8_t from_square = move.move & 0x3f;
            uint8_t to_square = (move.move >> 6) & 0x3f;
            uint8_t flag = (move.move >> 12) & 0x0f;

            if (from_sq == from_square && to_sq == to_square)
            {
                if (move_str.length() == 5)
                {
                    char promo_char = move_str[4];

                    if (promo_char == 'q' && !(flag == QUEEN_PROMOTION || flag == QUEEN_PROMOTION_AND_CAPTURE))
                        continue;
                    else if (promo_char == 'r' && !(flag == ROOK_PROMOTION || flag == ROOK_PROMOTION_AND_CAPTURE))
                        continue;
                    else if (promo_char == 'b' && !(flag == BISHOP_PROMOTION || flag == BISHOP_PROMOTION_AND_CAPTURE))
                        continue;
                    else if (promo_char == 'n' && !(flag == KNIGHT_PROMOTION || flag == KNIGHT_PROMOTION_AND_CAPTURE))
                        continue;
                }
                return move.move;
            }
        }

        // illegal move
        return 0;
    }
};

GameState game;

// return a mask for all squares where the rook can be blocked
uint64_t rook_attacks_mask(int square)
{
    uint64_t mask = 0;
    int rank = square / 8;
    int file = square % 8;

    // up
    for (int r = rank + 1; r <= 6; r++)
        mask |= (1ULL << (r * 8 + file));
    // down
    for (int r = rank - 1; r >= 1; r--)
        mask |= (1ULL << (r * 8 + file));
    // right
    for (int f = file + 1; f <= 6; f++)
        mask |= (1ULL << (rank * 8 + f));
    // left
    for (int f = file - 1; f >= 1; f--)
        mask |= (1ULL << (rank * 8 + f));

    return mask;
}
// return a mask for all squares where the bishop can be blocked
uint64_t bishop_attacks_mask(int square)
{

    int rank = square / 8;
    int file = square % 8;

    uint64_t mask = 0;

    // up right
    for (int r = rank + 1, f = file + 1; r < 7 && f < 7; r++, f++)
    {
        mask |= 1ull << (8 * r + f);
    }

    // up left
    for (int r = rank + 1, f = file - 1; r < 7 && f > 0; r++, f--)
    {
        mask |= 1ull << (8 * r + f);
    }

    // down right
    for (int r = rank - 1, f = file + 1; r > 0 && f < 7; r--, f++)
    {
        mask |= 1ull << (8 * r + f);
    }

    // down left
    for (int r = rank - 1, f = file - 1; r > 0 && f > 0; r--, f--)
    {
        mask |= 1ull << (8 * r + f);
    }

    return mask;
}
// return a mask for all squares where the queen can be blocked
uint64_t queen_attacks_mask(int square)
{
    return rook_attacks_mask(square) | bishop_attacks_mask(square);
}

// returns 1 combination of pieces that blocks the slider
uint64_t set_occupancy(int index, int bits_in_mask, uint64_t attack_mask)
{
    uint64_t occupancy = 0;
    for (int count = 0; count < bits_in_mask; count++)
    {
        int square = __builtin_ctzll(attack_mask);
        attack_mask &= attack_mask - 1;
        if (index & (1 << count))
        {
            occupancy |= 1LL << square;
        }
    }
    return occupancy;
}
// returns all possible moves for a rook for a given position(occupancy)
uint64_t calculate_rook_attacks(int square, uint64_t occupancy)
{
    uint64_t moves = 0;

    int rank = square / 8;
    int file = square % 8;

    // above
    for (int r = rank + 1; r <= 7; r++)
    {
        uint64_t sq = 1ull << (8 * r + file);
        moves |= sq;
        if (occupancy & sq)
            break;
    }

    // below
    for (int r = rank - 1; r >= 0; r--)
    {
        uint64_t sq = 1ull << (8 * r + file);
        moves |= sq;
        if (occupancy & sq)
            break;
    }

    // right
    for (int f = file + 1; f <= 7; f++)
    {
        uint64_t sq = 1ull << (8 * rank + f);
        moves |= sq;
        if (occupancy & sq)
            break;
    }

    // left
    for (int f = file - 1; f >= 0; f--)
    {
        uint64_t sq = 1ull << (8 * rank + f);
        moves |= sq;
        if (occupancy & sq)
            break;
    }

    return moves;
};
// returns all possible moves for a bishop for a given position(occupancy)
uint64_t calculate_bishop_attacks(int square, uint64_t occupancy)
{
    int rank = square / 8;
    int file = square % 8;

    uint64_t moves = 0;

    // up right
    for (int r = rank + 1, f = file + 1; r <= 7 && f <= 7; r++, f++)
    {
        uint64_t sq = 1ull << (8 * r + f);
        moves |= sq;
        if (occupancy & sq)
            break;
    }

    // up left
    for (int r = rank + 1, f = file - 1; r <= 7 && f >= 0; r++, f--)
    {
        uint64_t sq = 1ull << (8 * r + f);
        moves |= sq;
        if (occupancy & sq)
            break;
    }

    // down right
    for (int r = rank - 1, f = file + 1; r >= 0 && f <= 7; r--, f++)
    {
        uint64_t sq = 1ull << (8 * r + f);
        moves |= sq;
        if (occupancy & sq)
            break;
    }

    // down left
    for (int r = rank - 1, f = file - 1; r >= 0 && f >= 0; r--, f--)
    {
        uint64_t sq = 1ull << (8 * r + f);
        moves |= sq;
        if (occupancy & sq)
            break;
    }

    return moves;
}

// generating final attack masks for bishops
void init_bishop()
{
    for (int square = 0; square < 64; square++)
    {
        bishop_mask[square] = bishop_attacks_mask(square);
        int bits_in_mask = __builtin_popcountll(bishop_mask[square]);
        int combinations = 1 << bits_in_mask;

        for (int index = 0; index < combinations; index++)
        {
            uint64_t occupancy = set_occupancy(index, bits_in_mask, bishop_mask[square]);
            uint64_t legal_moves = calculate_bishop_attacks(square, occupancy);
            uint64_t magic_index = (occupancy * bishop_magic_number[square]) >> (64 - bits_in_mask);
            bishop_attacks[square][magic_index] = legal_moves;
        }
    }
}
// generating final attack masks for rooks
void init_rook()
{
    for (int square = 0; square < 64; square++)
    {
        rook_mask[square] = rook_attacks_mask(square);
        int bits_in_mask = __builtin_popcountll(rook_mask[square]);
        int combinations = 1 << bits_in_mask;

        for (int index = 0; index < combinations; index++)
        {
            uint64_t occupancy = set_occupancy(index, bits_in_mask, rook_mask[square]);
            uint64_t legal_moves = calculate_rook_attacks(square, occupancy);
            uint64_t magic_index = (occupancy * rook_magic_number[square]) >> (64 - bits_in_mask);
            rook_attacks[square][magic_index] = legal_moves;
        }
    }
}

void init_zhash()
{
    for (int piece = 0; piece < 12; piece++)
    {
        for (int sq = 0; sq < 64; sq++)
        {
            piece_keys[piece][sq] = rng();
        }
    }
    for (int sq = 0; sq < 64; sq++)
        enpassant_keys[sq] = rng();

    for (int rights = 0; rights < 16; rights++)
        castle_keys[rights] = rng();

    side_key = rng();
}

void init_lmr()
{
    for (int depth = 0; depth < 64; depth++)
    {
        for (int move_num = 0; move_num < 512; move_num++)
        {
            lmr_table[depth][move_num] = (log2(depth + 1) * log2(move_num + 1)) / 2;
        }
    }
}

void init_all()
{
    init_bishop();
    init_rook();
    init_zhash();
    init_tables();
    init_lmr();
 
    for (int i = 0; i < 64; i++)
    {
        uint64_t piece = 1ULL << i;

        uint64_t n_ur = (piece & NOT_H_FILE) << 17;
        uint64_t n_ul = (piece & NOT_A_FILE) << 15;
        uint64_t n_dr = (piece & NOT_H_FILE) >> 15;
        uint64_t n_dl = (piece & NOT_A_FILE) >> 17;

        uint64_t n_ru = (piece & NOT_GH_FILE) << 10;
        uint64_t n_rd = (piece & NOT_GH_FILE) >> 6;
        uint64_t n_lu = (piece & NOT_AB_FILE) << 6;
        uint64_t n_ld = (piece & NOT_AB_FILE) >> 10;

        knight_attacks[i] = n_ur | n_ul | n_dr | n_dl | n_ru | n_rd | n_lu | n_ld;

        uint64_t k_ur = (piece & NOT_H_FILE) << 9;
        uint64_t k_ul = (piece & NOT_A_FILE) << 7;
        uint64_t k_dr = (piece & NOT_H_FILE) >> 7;
        uint64_t k_dl = (piece & NOT_A_FILE) >> 9;

        uint64_t k_u = (piece) << 8;
        uint64_t k_d = (piece) >> 8;
        uint64_t k_r = (piece & NOT_H_FILE) << 1;
        uint64_t k_l = (piece & NOT_A_FILE) >> 1;

        king_attacks[i] = k_ur | k_ul | k_dr | k_dl | k_u | k_d | k_r | k_l;
    }
    // generating attack masks for white pawns
    for (int i = 0; i < 64; i++)
    {
        uint64_t piece = 1ULL << i;
        uint64_t ur = (piece & NOT_H_FILE) << 9;
        uint64_t ul = (piece & NOT_A_FILE) << 7;
        white_pawn_attacks[i] = ur | ul;
    }
    // generating attack masks for black pawns
    for (int i = 0; i < 64; i++)
    {
        uint64_t piece = 1ULL << i;
        uint64_t dr = (piece & NOT_H_FILE) >> 7;
        uint64_t dl = (piece & NOT_A_FILE) >> 9;
        black_pawn_attacks[i] = dr | dl;
    }
}

std::string print_move(uint16_t move)
{
    std::string m = "";
    int from = move & 0x3f;
    int to = (move >> 6) & 0x3f;
    int flag = (move >> 12) & 0x0f;

    char files[] = "abcdefgh";
    char ranks[] = "12345678";

    m += files[from % 8];
    m += ranks[from / 8];
    m += files[to % 8];
    m += ranks[to / 8];

    // Print promotion pieces if applicable
    if (flag >= 2 && flag <= 5)
    {
        char promos[] = "xnbrq"; // offset padding
        m += promos[flag - 1];
    }
    else if (flag >= 6 && flag <= 9)
    {
        char promos[] = "xnbrq";
        m += promos[flag - 5];
    }

    return m;
}

void hash_perft(int depth, GameState &game1)
{
    if (!depth)
        return;
    MoveList list;
    game1.generate_move(list, depth);
    for (int i = 0; i < list.move_count; i++)
    {
        uint16_t move = list.moves_list[i].move;

        int ep_copy = game1.ep_square;
        int castle_copy = game1.castling_right;

        u_int64_t saved_hash = game1.current_hash;
        game1.make_move(move);

        // check if move is legal
        int current_side = game1.side_to_move ^ 1;
        int sq = __builtin_ctzll(game1.bitboard[current_side ? BLACK_KING : WHITE_KING]);
        if (game1.is_square_attacked(sq, !current_side))
        {
            game1.unmake_move(move);

            game1.ep_square = ep_copy;
            game1.castling_right = castle_copy;

            continue;
        }

        hash_perft(depth - 1, game1);
        game1.unmake_move(move);
        if (saved_hash != game1.current_hash)
        {
            std::cout << "CRITICAL HASH DESYNC!\n";
            std::cout << "Move causing issue: ";
            std::cout << print_move(move);
            std::cout << "\n";
            std::cout << "before: " << saved_hash << "\n";
            std::cout << " after: " << current_side << "\n";
            // Optional: print out the board state or the before/after hashes here
            exit(1);
        }

        game1.ep_square = ep_copy;
        game1.castling_right = castle_copy;
    }
    return;
}

int quiescence(GameState &game, int alpha, int beta, int depth)
{
    NODES++;
    int stand_pat = game.evaluation();
    if (stand_pat >= beta)
        return stand_pat;
    if (stand_pat > alpha)
        alpha = stand_pat;

    MoveList list;
    game.generate_move(list, depth);

    std::sort(list.moves_list, list.moves_list + list.move_count, [](Move &a, Move &b)
              { return a.score > b.score; });

    int legal_moves = 0;

    for (int i = 0; i < list.move_count; i++)
    {
        uint16_t move = list.moves_list[i].move;
        uint8_t flag = (move >> 12) & 0x0f;

        bool isCapture = (flag == STANDARD_CAPTURE || flag == EN_PASSANT || (flag >= KNIGHT_PROMOTION_AND_CAPTURE && flag <= QUEEN_PROMOTION_AND_CAPTURE));

        if (!isCapture)
            continue;

        int ep_copy = game.ep_square;
        int castle_copy = game.castling_right;

        game.make_move(move);

        // check if move is legal
        int current_side = game.side_to_move ^ 1;
        int sq = __builtin_ctzll(game.bitboard[current_side ? BLACK_KING : WHITE_KING]);
        if (game.is_square_attacked(sq, !current_side))
        {
            game.unmake_move(move);
            game.ep_square = ep_copy;
            game.castling_right = castle_copy;
            continue;
        }
        legal_moves++;
        // NODES++;
        int score = -quiescence(game, -beta, -alpha, depth);

        game.unmake_move(move);
        game.ep_square = ep_copy;
        game.castling_right = castle_copy;

        if (stop_search) return 0;

        if (score >= beta)
            return score;
        alpha = std::max(alpha, score);
    }

    return alpha;
}

int pvs_research = 0;
int nigamax(int depth, GameState &game, int alpha, int beta, bool allow_null = true)
{
    NODES++;
    if ((NODES & (2047)) == 0) 
    {
        if ((std::chrono::high_resolution_clock::now() - timer_start) >= hard_time) 
        {
            stop_search = true;
            return 0;
        }
    }
    if (game.is_repetition()) return 0;
    if (!depth)
        // return game.evaluation();
        return quiescence(game, alpha, beta, depth);

    TTentry entry;
    entry.hash = game.current_hash;
    entry.depth = depth;
    MoveList list;
    Move tt_move = {0, 0};
    uint32_t tt_hash = game.current_hash & (TTsize - 1);

    if (transposition_table[tt_hash].hash)
    {
        TTentry temp_entry = transposition_table[tt_hash];
        if (temp_entry.hash == game.current_hash && temp_entry.depth >= depth)
        {
            if (temp_entry.flag == HASH_EXACT)
            {
                return temp_entry.score;
            }
            else if (temp_entry.flag == HASH_BETA && temp_entry.score >= beta)
            {
                return temp_entry.score; // Lower bound proves it's too good for the opponent. Cutoff.
            }
            else if (temp_entry.flag == HASH_ALPHA && temp_entry.score <= alpha)
            {
                return temp_entry.score; // Upper bound proves it's terrible for us. Cutoff.
            }
        }
        if (temp_entry.hash == game.current_hash && temp_entry.best_move != 0)
        {
            tt_move.move = temp_entry.best_move;
            list.moves_list[list.move_count++] = {tt_move.move, 200000};
        }
        // list.moves_list[list.move_count++] = temp_entry.best_move;
    }
    // select reduction factor
    int nmp_reduce = 2;
    if (depth >= 12)
        nmp_reduce = depth - 10;
    else if (alpha >= 700)
        nmp_reduce = 5;
    int MIN_NULL_DEPTH = nmp_reduce + 2;

    // zugzwang check
    if (allow_null && depth >= MIN_NULL_DEPTH && game.has_non_pawn_material())
    {
        // check if move is legal
        int current_side = game.side_to_move;
        int sq = __builtin_ctzll(game.bitboard[current_side ? BLACK_KING : WHITE_KING]);
        if (!game.is_square_attacked(sq, !current_side))
        {
            game.make_null_move();
            int score = -nigamax(depth - 1 - nmp_reduce, game, -beta, -beta + 1, false);

            game.unmake_null_move();

            if (stop_search) return 0;

            if (score >= beta)
            {
                return score;
            }
        }
    }

    game.generate_move(list, depth, tt_move.move);

    std::sort(list.moves_list + (tt_move.move ? 1 : 0), list.moves_list + list.move_count, [](Move &a, Move &b)
              { return a.score > b.score; });

    int legal_moves = 0;
    int originalAlpha = alpha;
    Move best_move{0, -1200000};
    for (int move_num = 0; move_num < list.move_count; move_num++)
    {
        uint16_t move = list.moves_list[move_num].move;

        int ep_copy = game.ep_square;
        int castle_copy = game.castling_right;

        game.make_move(move);

        // check if move is legal
        int current_side = game.side_to_move ^ 1;
        int sq = __builtin_ctzll(game.bitboard[current_side ? BLACK_KING : WHITE_KING]);
        if (game.is_square_attacked(sq, !current_side))
        {
            game.unmake_move(move);
            game.ep_square = ep_copy;
            game.castling_right = castle_copy;
            continue;
        }
        legal_moves++;
        // NODES++;
        
        // LMR
        int score = 0;
        // score = -nigamax(depth - 1, game, -beta, -alpha, true);
        uint8_t flag = (move >> 12) & 0x0f;
        bool isCapture = (flag == STANDARD_CAPTURE || flag == EN_PASSANT || (flag >= KNIGHT_PROMOTION_AND_CAPTURE && flag <= QUEEN_PROMOTION_AND_CAPTURE));
        bool isPromotion = (flag >= KNIGHT_PROMOTION && flag <= QUEEN_PROMOTION);
        bool isKiller = (move == game.killer_moves[depth][0] || move == game.killer_moves[depth][1]);
        bool isCheck = game.is_square_attacked(__builtin_ctzll(game.bitboard[game.side_to_move ? BLACK_KING : WHITE_KING]), game.side_to_move ^ 1);

        int extension = isCheck ? 1 : 0;
        int new_depth = depth - 1 + extension;

        if (legal_moves == 1)
        {
            score = -nigamax(new_depth, game, -beta, -alpha, true);
        }
        else 
        {
            bool do_full_pvs = true;
            if (depth >= 3 && legal_moves >= 4 && !isCapture && !isPromotion && !isKiller && !isCheck)
            {
                int lmr_reduce = lmr_table[depth][legal_moves];
                int reduced_depth = std::max(1, new_depth - lmr_reduce);
                score = -nigamax(reduced_depth, game, -alpha - 1, -alpha, true);

                if (score <= alpha) 
                {
                    do_full_pvs = false;
                }
            }
            if (do_full_pvs) 
            {
                score = -nigamax(new_depth, game, -alpha - 1, -alpha, true);
                if (score > alpha && score < beta)
                {
                    pvs_research++;
                    score = -nigamax(new_depth, game, -beta, -alpha, true);
                }
            } 
        }

        game.unmake_move(move);
        game.ep_square = ep_copy;
        game.castling_right = castle_copy;

        if (stop_search) return 0;

        if (score > best_move.score)
        {
            best_move = {move, score};
        }
        if (score >= beta)
        {
            entry.score = score;
            entry.flag = HASH_BETA;
            best_move = {move, score};
            entry.best_move = best_move.move;
            tt_replace(tt_hash, entry);
            int8_t flag = (move >> 12) & 0x0f;
            if (flag == STANDARD_QUIET_MOVE || flag == KINGSIDE_CASTLE || flag == QUEENSIDE_CASTLE || (flag >= KNIGHT_PROMOTION && flag <= QUEEN_PROMOTION))
            {
                if (game.killer_moves[depth][0] != move)
                {
                    // update killer move
                    game.killer_moves[depth][1] = game.killer_moves[depth][0]; // Shift old #1 to #2
                    game.killer_moves[depth][0] = move;                        // Save new #1

                    // update history table
                    uint8_t from_square = move & 0x3f;
                    uint8_t to_square = (move >> 6) & 0x3f;
                    int piece = game.piece_on_square[from_square];
                    game.history_table[piece][to_square] += (depth * depth);
                }
            }

            return score;
        }
        if (score > alpha)
        {
            alpha = score;
            best_move.move = move;
        }
        // alpha = std::max(alpha, score);
    }

    if (!legal_moves)
    {
        if (game.is_square_attacked(__builtin_ctzll(game.bitboard[game.side_to_move ? BLACK_KING : WHITE_KING]), game.side_to_move ^ 1))
        {
            entry.score = -900000 - depth;
            entry.flag = HASH_EXACT;
            entry.best_move = 0;
            tt_replace(tt_hash, entry);
            return -900000 - depth;
        }
        entry.score = 0;
        entry.flag = HASH_EXACT;
        entry.best_move = 0;
        tt_replace(tt_hash, entry);
        return 0;
    }
    entry.score = alpha;
    entry.flag = alpha > originalAlpha ? HASH_EXACT : HASH_ALPHA;

    entry.best_move = best_move.move;
    tt_replace(tt_hash, entry);
    return alpha;
}

int32_t root(int depth, GameState &game, int alpha, int beta, uint16_t &best_move_out)
{
    NODES++;
    TTentry entry;
    entry.hash = game.current_hash;
    entry.depth = depth;
    MoveList list;
    Move tt_move = {0, 0};
    int32_t tt_hash = game.current_hash & (TTsize - 1);

    if (transposition_table[tt_hash].hash)
    {
        TTentry temp_entry = transposition_table[tt_hash];
        if (temp_entry.hash == game.current_hash && temp_entry.depth >= depth)
        {
            if (temp_entry.flag == HASH_EXACT)
            {
                best_move_out =  temp_entry.best_move;
                return temp_entry.score;
            }
            else if (temp_entry.flag == HASH_BETA && temp_entry.score >= beta)
            {
                return temp_entry.score; // Lower bound proves it's too good for the opponent. Cutoff.
            }
            else if (temp_entry.flag == HASH_ALPHA && temp_entry.score <= alpha)
            {
                return temp_entry.score; // Upper bound proves it's terrible for us. Cutoff.
            }
            
        }
        if (temp_entry.hash == game.current_hash && temp_entry.best_move != 0)
        {
            tt_move. move = temp_entry.best_move;
            list.moves_list[list.move_count++] = {tt_move.move, 200000};
        }
        // list.moves_list[list.move_count++] = temp_entry.best_move;
    }

    game.generate_move(list, depth, tt_move.move);

    std::sort(list.moves_list + (tt_move.move ? 1 : 0), list.moves_list + list.move_count, [&](Move a, Move b)
              { return a.score > b.score; });

    int legal_moves = 0;
    int originalAlpha = alpha;
    Move best_move{list.moves_list[0].move, -1200000};
    uint16_t best_move_print = 0;
    // uint16_t best_move_print = best_move.move;
    for (int i = 0; i < list.move_count; i++)
    {
        uint16_t move = list.moves_list[i].move;
        int ep_copy = game.ep_square;
        int castle_copy = game.castling_right;

        game.make_move(move);

        // check if move is legal
        int current_side = game.side_to_move ^ 1;
        int sq = __builtin_ctzll(game.bitboard[current_side ? BLACK_KING : WHITE_KING]);
        if (game.is_square_attacked(sq, !current_side))
        {
            game.unmake_move(move);
            game.ep_square = ep_copy;
            game.castling_right = castle_copy;
            continue;
        }
        legal_moves++;
        // std::cout << print_move(move) << " " << legal_moves << std::endl;
        // NODES++;
        int score = 0;
        bool isCheck = game.is_square_attacked(__builtin_ctzll(game.bitboard[game.side_to_move ? BLACK_KING : WHITE_KING]), game.side_to_move ^ 1);

        int extension = isCheck ? 1 : 0;
        int new_depth = depth - 1 + extension;
        
        // score = -nigamax(new_depth, game, -beta, -alpha);
        // score = -nigamax(depth - 1, game, -beta, -alpha);

        // PVS
        if (legal_moves == 1)
        {
            score = -nigamax(new_depth, game, -beta, -alpha, true);
        }
        else 
        {
            score = -nigamax(new_depth, game, -alpha - 1, -alpha, true);
            if (score > alpha && score < beta)
            {
                pvs_research++;
                score = -nigamax(new_depth, game, -beta, -alpha, true);
            }
        }

        game.unmake_move(move);
        game.ep_square = ep_copy;
        game.castling_right = castle_copy;

        if (stop_search) return 0;

        if (score > best_move.score)
        {
            best_move = {move, score};
        }
        if (score >= beta)
        {
            // std::cout << print_move(move) << " " << legal_moves << " " << score << " " << beta << std::endl;
            entry.score = score;
            entry.flag = HASH_BETA;
            best_move = {move, score};
            entry.best_move = best_move.move;
            tt_replace(tt_hash, entry);
            best_move_out =  best_move.move;
            return score;
        }
        // std::cout << print_move(move) << " score: " << score << " alpha: " << alpha << std::endl;
        if (score > alpha)
        {
            // std::cout << print_move(move) << " " << legal_moves << " " << score << " " << alpha << std::endl;
            alpha = score;
            best_move.move = move;
            best_move_print = move;
        }

        // alpha = std::max(alpha, score);
    }

    if (!legal_moves)
    {
        if (game.is_square_attacked(__builtin_ctzll(game.bitboard[game.side_to_move ? BLACK_KING : WHITE_KING]), game.side_to_move ^ 1))
        {
            entry.score = -900000 - depth;
            entry.flag = HASH_EXACT;
            entry.best_move = 0;
            tt_replace(tt_hash, entry);
            best_move_out = 0;
            return -900000 - depth;
        } // FFFF TTTT TTFF FFFFF
        entry.score = -900000 - depth;
        entry.flag = HASH_EXACT;
        entry.best_move = 0;
        tt_replace(tt_hash, entry);
        best_move_out = 0;
        return 0;
    }

    entry.score = alpha;
    entry.flag = alpha > originalAlpha ? HASH_EXACT : HASH_ALPHA;

    std::cout << "Best score    : " << alpha << "\n";
    entry.best_move = best_move.move;
    tt_replace(tt_hash, entry);
    best_move_out = legal_moves ? best_move_print : 0;

    return alpha;
}

#define empty_board "8/8/8/8/8/8/8/8 w - -"
#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define kiwipete_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -"
#define position3 "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
#define position5 "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
#define position6 "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9"
#define test_position "8/k7/3n4/2pB4/3p3K/P7/1PP2PPP/8 b - - 0 127"

void set_position(std::string fen, std::vector<std::string> &moves)
{
    // Parse FEN, set up the board, make the moves
    // game = GameState();
    game.reset_board();

    game.fen_parser(fen);
    game.init_eval();
    game.current_hash = game.generate_hash(); // ensure hash is fresh
    for (auto &m : moves)
    {
        uint16_t uci_move = game.parse_uci_move(m, 0);
        if (uci_move)
            game.make_move(uci_move);
    }
}

void search(int depth, int movetime, int wtime, int btime, int winc, int binc)
{
    int max_depth = 100; // max number of plys to search for
    int soft_time_limit_ms = 36000000; // optimal time allowed to play the current move
    int hard_time_limit_ms = 36000000; // absolute max time allowed to play the current move
    int buffer = 50; // for latency and overhead to avoid acciedental flagging
    
    if (depth > 0) max_depth = depth;
    if (movetime > 0) 
    {
        soft_time_limit_ms = movetime - buffer;
        hard_time_limit_ms = movetime - buffer; 
    }  
    if (wtime > 0 && btime > 0) 
    {
        int safe_divisor = 40; // an expected safe number of moves that are left to play
        int absolute_divisor = 10; // an expected min number of moves left to play in a complex position
        int total_time = game.side_to_move ? btime : wtime; // total time left
        int inc = game.side_to_move ? binc : winc; // increament per move

        soft_time_limit_ms = (total_time / safe_divisor) + static_cast<int>(inc * 0.7) - buffer;
        hard_time_limit_ms = (total_time / absolute_divisor) - buffer;
        
        if (soft_time_limit_ms > total_time - buffer) soft_time_limit_ms = total_time - buffer;
        if (soft_time_limit_ms < 15) soft_time_limit_ms = 15;

        if (hard_time_limit_ms > total_time - buffer) hard_time_limit_ms = total_time - buffer;
        if (hard_time_limit_ms < soft_time_limit_ms) hard_time_limit_ms = soft_time_limit_ms;
        if (hard_time_limit_ms < 15) hard_time_limit_ms = 15;
    }
    int previous_score = 0;
    uint16_t best_move = 0;
    stop_search = false;
    timer_start = std::chrono::high_resolution_clock::now();
    soft_time = std::chrono::milliseconds(soft_time_limit_ms);
    hard_time = std::chrono::milliseconds(hard_time_limit_ms);

    for (int curr_depth = 1; curr_depth <= max_depth; curr_depth++)
    {
        std::cout << "Depth         : " << curr_depth << "\n";
        auto start = std::chrono::high_resolution_clock::now();

        int window = 50; // 50 centipawns (half a pawn)
        int alpha = previous_score - window;
        int beta = previous_score + window;

        uint16_t curr_move = 0;
        int32_t current_score = 0;

        while (true) 
        {
            if (curr_depth <= 3) 
            {
                alpha = -1000000;
                beta = 1000000;
            }

            current_score = root(curr_depth, game, alpha , beta, curr_move); // 2ms
            if (stop_search) 
            {   
                int64_t total_time = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timer_start)).count();
                std::cout << "Total time: " << total_time << " ms\n\n";
                break; 
            }

            if (current_score <= alpha)
            {
                alpha = -1000000;
                continue; 
            }
            else if (current_score >= beta)
            {
                beta = 1000000;
                continue;
            }
            break;
        }
        if (stop_search) 
        {   
            break; 
        }
        best_move = curr_move;
        previous_score = current_score;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto time = duration.count();

        std::cout << "Engine plays  : " << print_move(curr_move) << "\n";
        std::cout << "Nodes explored: " << NODES << "\n";
        std::cout << "Time taken    : " << time << " ms\n";
        if (time != 0) std::cout << "Speed         : " << NODES / time << " nodes/ms\n\n";
        else std::cout << "Speed         : " << NODES << " nodes/ms\n\n";

        if (time * 2 > soft_time_limit_ms) break;
    }
    std::cout << "bestmove " << print_move(best_move) << std::endl;
}

//void search_fixed_depth(int depth)
//{
//    // Run minimax to 'depth'
//    // std::cout << "bestmove " << best_move << "\n";
//    // uint16_t best_move = root(depth, game);
//    int max_depth = depth;
//    int previous_score = 0;
//
//    stop_search = false;
//    timer_start = std::chrono::high_resolution_clock::now();
//    max_time = std::chrono::milliseconds(36000000);
//    
//    uint16_t best_move = 0;
//    
//    for (int curr_depth = 1; curr_depth <= max_depth; curr_depth++)
//    {
//        std::cout << "Depth         : " << curr_depth << "\n";
//        auto start = std::chrono::high_resolution_clock::now();
//
//        int window = 50; // 50 centipawns (half a pawn)
//        int alpha = previous_score - window;
//        int beta = previous_score + window;
//
//        uint16_t curr_move = 0;
//        int32_t current_score = 0;
//
//        while (true) 
//        {
//            if (curr_depth <= 3) 
//            {
//                alpha = -1000000;
//                beta = 1000000;
//            }
//
//            current_score = root(curr_depth, game, alpha , beta, curr_move); // 2ms
//
//            if (current_score <= alpha)
//            {
//                alpha = -1000000;
//                continue; 
//            }
//            else if (current_score >= beta)
//            {
//                beta = 1000000;
//                continue;
//            }
//            break;
//        }
//        
//        best_move = curr_move;
//        previous_score = current_score;
//
//        auto end = std::chrono::high_resolution_clock::now();
//        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//        auto time = duration.count();
//
//        std::cout << "Engine plays  : " << print_move(curr_move) << "\n";
//        std::cout << "Nodes explored: " << NODES << "\n";
//        std::cout << "Time taken    : " << time << " ms\n";
//        if (time != 0) std::cout << "Speed         : " << NODES / time << " nodes/ms\n\n";
//        else std::cout << "Speed         : " << NODES << " nodes/ms\n\n";
//    }
//    std::cout << "bestmove " << print_move(best_move) << std::endl;
//}
//
//void search_fixed_time(int time_ms)
//{ 
//    int max_depth = 20;
//    int previous_score = 0;
//    
//    uint16_t best_move = 0;
//    stop_search = false;
//    timer_start = std::chrono::high_resolution_clock::now();
//    max_time = std::chrono::milliseconds(time_ms - 50);
//
//    for (int curr_depth = 1; curr_depth <= max_depth; curr_depth++)
//    {
//        std::cout << "Depth         : " << curr_depth << "\n";
//        auto start = std::chrono::high_resolution_clock::now();
//
//        int window = 50; // 50 centipawns (half a pawn)
//        int alpha = previous_score - window;
//        int beta = previous_score + window;
//
//        uint16_t curr_move = 0;
//        int32_t current_score = 0;
//
//        while (true) 
//        {
//            if (curr_depth <= 3) 
//            {
//                alpha = -1000000;
//                beta = 1000000;
//            }
//
//            current_score = root(curr_depth, game, alpha , beta, curr_move); // 2ms
//            if (stop_search) 
//            {   
//                int64_t total_time = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timer_start)).count();
//                std::cout << "Total time: " << total_time << " ms\n\n";
//                break; 
//            }
//
//            if (current_score <= alpha)
//            {
//                alpha = -1000000;
//                continue; 
//            }
//            else if (current_score >= beta)
//            {
//                beta = 1000000;
//                continue;
//            }
//            break;
//        }
//        if (stop_search) 
//        {   
//            break; 
//        }
//        best_move = curr_move;
//        previous_score = current_score;
//
//        auto end = std::chrono::high_resolution_clock::now();
//        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//        auto time = duration.count();
//
//        std::cout << "Engine plays  : " << print_move(curr_move) << "\n";
//        std::cout << "Nodes explored: " << NODES << "\n";
//        std::cout << "Time taken    : " << time << " ms\n";
//        if (time != 0) std::cout << "Speed         : " << NODES / time << " nodes/ms\n\n";
//        else std::cout << "Speed         : " << NODES << " nodes/ms\n\n";
//    }
//    std::cout << "bestmove " << print_move(best_move) << std::endl;
//}
//
//void search_time_control(int wtime, int btime, int winc, int binc)
//{
//    // to be done
//    // Calculate how much time to spend based on whose turn it is
//    // Run minimax for that amount of time
//    
//    int total_time = game.side_to_move ? btime : wtime; // total time left
//    int inc = game.side_to_move ? binc : winc;// increment per move
//    
//    int base_divisor = 40; // an expected safe number of moves that are left to play
//    int buffer = 50; // for latency and overhead to avoid acciedental flagging
//    int time_limit_ms = (total_time / base_divisor) + inc - buffer; // time allowed to play the current move
//
//    int max_depth = 40;
//    int previous_score = 0;
//    
//    uint16_t best_move = 0;
//    stop_search = false;
//    timer_start = std::chrono::high_resolution_clock::now();
//    max_time = std::chrono::milliseconds(time_limit_ms);
//
//    for (int curr_depth = 1; curr_depth <= max_depth; curr_depth++)
//    {
//        std::cout << "Depth         : " << curr_depth << "\n";
//        auto start = std::chrono::high_resolution_clock::now();
//
//        int window = 50; // 50 centipawns (half a pawn)
//        int alpha = previous_score - window;
//        int beta = previous_score + window;
//
//        uint16_t curr_move = 0;
//        int32_t current_score = 0;
//
//        while (true) 
//        {
//            if (curr_depth <= 3) 
//            {
//                alpha = -1000000;
//                beta = 1000000;
//            }
//
//            current_score = root(curr_depth, game, alpha , beta, curr_move); // 2ms
//            if (stop_search) 
//            {   
//                int64_t total_time = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timer_start)).count();
//                std::cout << "Total time: " << total_time << " ms\n\n";
//                break; 
//            }
//
//            if (current_score <= alpha)
//            {
//                alpha = -1000000;
//                continue; 
//            }
//            else if (current_score >= beta)
//            {
//                beta = 1000000;
//                continue;
//            }
//            break;
//        }
//        if (stop_search) 
//        {   
//            break; 
//        }
//        best_move = curr_move;
//        previous_score = current_score;
//
//        auto end = std::chrono::high_resolution_clock::now();
//        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//        auto time = duration.count();
//
//        std::cout << "Engine plays  : " << print_move(curr_move) << "\n";
//        std::cout << "Nodes explored: " << NODES << "\n";
//        std::cout << "Time taken    : " << time << " ms\n";
//        if (time != 0) std::cout << "Speed         : " << NODES / time << " nodes/ms\n\n";
//        else std::cout << "Speed         : " << NODES << " nodes/ms\n\n";
//    }
//    std::cout << "bestmove " << print_move(best_move) << std::endl;
//    // std::cout << "bestmove " << best_move << "\n";
//}

// uint16_t run1(int max_depth)
// {
    
//     int max_depth = 20;
//     int previous_score = 0;
    
//     uint16_t best_move = 0;
    
//     for (int curr_depth = 1; curr_depth <= max_depth; curr_depth++)
//     {
//         auto start = std::chrono::high_resolution_clock::now();

//         int window = 50; // 50 centipawns (half a pawn)
//         int alpha = previous_score - window;
//         int beta = previous_score + window;

//         uint16_t curr_move = 0;
//         int32_t current_score = 0;

//         while (true) 
//         {
//             if (curr_depth <= 3) 
//             {
//                 alpha = -1000000;
//                 beta = 1000000;
//             }

//             current_score = root(curr_depth, game, alpha , beta, curr_move); // 2ms

//             if (current_score <= alpha)
//             {
//                 alpha = -1000000;
//                 continue; 
//             }
//             else if (current_score >= beta)
//             {
//                 beta = 1000000;
//                 continue;
//             }
//             break;
//         }
        
//         best_move = curr_move;
//         previous_score = current_score;

//         auto end = std::chrono::high_resolution_clock::now();
//         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//         auto time = duration.count();

//         std::cout << "Depth         : " << curr_depth << "\n";
//         std::cout << "Engine plays  : " << print_move(curr_move) << "\n";
//         std::cout << "Nodes explored: " << NODES << "\n";
//         std::cout << "Time taken    : " << time << " ms\n";
//         if (time != 0) std::cout << "Speed         : " << NODES / time << " nodes/ms\n\n";
//         else std::cout << "Speed         : " << NODES << " nodes/ms\n\n";
//     }
//     return best_move;
// }

// void test(GameState &game)
// {
//     int max_depth = 20;
//     int previous_score = 0;
    

//     for (int curr_depth = 1; curr_depth <= max_depth; curr_depth++)
//     {
//         std::cout << "Depth         : " << curr_depth << "\n";
//         // std::cout << "Thinking...\n";

//         auto start = std::chrono::high_resolution_clock::now();

//         int window = 50; // 50 centipawns (half a pawn)
//         int alpha = previous_score - window;
//         int beta = previous_score + window;

//         uint16_t best_move = 0;
//         int32_t current_score = 0;

//         while (true) {
//             if (curr_depth <= 3) 
//             {
//                 alpha = -1000000;
//                 beta = 1000000;
//             }

//             current_score = root(curr_depth, game, alpha , beta, best_move); // 2ms

//             if (current_score <= alpha)
//             {
//                 alpha = -1000000;
//                 continue; 
//             }
//             else if (current_score >= beta)
//             {
//                 beta = 1000000;
//                 continue;
//             }

//             break;
//         }

//         previous_score = current_score;

//         // uint16_t best_move = find_best_move(depth, game1); // 393ms
//         auto end = std::chrono::high_resolution_clock::now();
//         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//         auto time = duration.count();

//         std::cout << "Engine plays  : " << print_move(best_move) << "\n";
//         std::cout << "Nodes explored: " << NODES << "\n";
//         std::cout << "Time taken    : " << time << " ms\n";
//         if (time != 0) std::cout << "Speed         : " << NODES / time << " nodes/ms\n\n";
//         else std::cout << "Speed         : " << NODES << " nodes/ms\n\n";
//     }
// }

// int main()
// {
//     // print_magics();
//     init_all();
//     // GameState game1{"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R"};
//     // GameState game("r6k/6pp/8/p4p2/P2Q1P1P/1P2n1P1/4q3/6K1 w - - 1 37");
//     // GameState game("7k/6pp/8/p4p1P/P4P2/1r2q1P1/6K1/8 w - - 0 40");
//     GameState game(start_position);
//     // GameState game("2Q5/5rkR/4p1p1/3pP1P1/rq3PK1/3P4/8/5R2 b - - 0 37");
//     // GameState game(killer_position);
//     // GameState game("6k1/8/8/8/8/8/q6p/4K3 w - - 7 26");
//     // GameState game("4k3/8/8/q7/7r/P2P4/3r1PPP/6K1 w - - 0 1");
//     // GameState game("8/6pk/p6p/4P3/8/1r2PR1P/6P1/6K1 w - - 0 36");
//     // GameState game("8/6pk/p6p/4P3/4P3/1r3R1P/6P1/6K1 b - - 0 36");

//     test(game);
//     std::cout << "re-searches    : " << pvs_research << " \n";

//     return 0;
// }

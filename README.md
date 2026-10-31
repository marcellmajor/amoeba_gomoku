# Amoeba: fixed version

`amoeba.cpp` is a corrected copy of the original five-in-a-row game in `AMOEBA.C`. You play **X** and the computer plays **O** on a 24 × 23 board. Five or more in a row wins. The computer scores every empty square for attack and for defence, as the original did. It also looks ahead for forced wins: it plays its own when it finds one, and blocks yours.

The original `AMOEBA.C` and `conio.h` are unchanged.

## Files

| File | What it is |
|---|---|
| `amoeba.cpp` | The fixed game. |
| `conio_fixed.h` | A fixed copy of `conio.h`, the terminal layer `amoeba.cpp` uses. |
| `tests/test_amoeba.cpp` | Unit tests: scoring, winner detection, move choice, saving and loading. |
| `tests/pty_test.py` | End-to-end tests that play the real program through a pseudo-terminal. |
| `AMOEBA.C`, `conio.h` | The original program, untouched. |
| `save1` … `save5` | Save slots. The originals load in the new version unchanged. |

## Building

You need a C++ compiler and a POSIX terminal (macOS or Linux). The terminal layer is a C++ class, so build the program as C++.

```bash
clang++ -std=c++11 -Wall -Wextra -O2 -o amoeba amoeba.cpp
```

On Linux, `g++` takes the same flags. The program compiles with no warnings under `-Wall -Wextra -Wpedantic`.

## Running

```bash
./amoeba
```

Use a terminal of at least 80 × 25. The game reads and writes `save1` … `save5` in the current folder.

| Key | Action |
|---|---|
| Arrow keys or `w` `a` `s` `d` | Move the cursor |
| `Space` | Place an X on the empty square under the cursor |
| `o` | Save: pick a slot 1–5, then type a name (`a`–`z`, up to 8 letters; Enter ends it and may leave it empty) |
| `l` | Load slot 1–5 |
| `q` | Abort the current game |
| `y` / `n` | After a game: play another one or exit |

The computer breaks ties between equally good squares at random. It seeds the random generator once at start-up from the clock. To get repeatable games, set a fixed seed:

```bash
AMOEBA_SEED=42 ./amoeba
```

`AMOEBA_SEED` must be a decimal number; any other value is ignored and the clock is used.

The program exits with status 0 after `n`. It exits with status 1 if its input is closed (end of file), where the original spun forever.

## Running the tests

Build and run the unit tests with AddressSanitizer and UndefinedBehaviorSanitizer. The argument is the folder holding the original saves; the tests copy them to a temporary folder and never write in the project.

```bash
clang++ -std=c++11 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -o /tmp/test_amoeba tests/test_amoeba.cpp
```

```bash
/tmp/test_amoeba .
```

Build the game with the sanitizers and play it through a pseudo-terminal. Each scenario runs in its own temporary folder.

```bash
clang++ -std=c++11 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -o /tmp/amoeba_san amoeba.cpp
```

```bash
python3 tests/pty_test.py /tmp/amoeba_san .
```

| Test file | What it covers |
|---|---|
| `test_amoeba.cpp` (553 checks) | Empty-board evaluation. Contiguous and single-gap patterns on all four axes. Edges and all four corners. How combinations of fours and threes rank, and the move chosen between a four-three and a double three. Four positions where the greedy choice loses and the lookahead does not: its own four-three against your open three, its own double three against your triple-three square, which end of your open three to block, and your forced win through threes that the greedy choice blocks on the wrong square. A block that also makes its own four. A block check that runs out of steps is reported as unknown, not safe. The block check plays out forced replies in turn order (its four, your block, its block of your four), and puts the board back on every path. The counter for marking squares wraps around without overflowing. Every forced win the search finds on 150 random positions, played out against every reply. Every arrangement of the 8 squares within reach of a square, next to every edge and corner (422,212 cases): only defined pattern codes, no "free four with 5 squares of room", and every live pattern has a score. Random boards evaluated everywhere under the sanitizers. Winner detection on a board other than the global one. Win before draw on a full board. Zero-score legal moves and the no-legal-move case. Placement that refuses to overwrite. The tie-breaking distribution. Seeding. Save round trips, including empty names. The original saves load and rewrite byte for byte. 27 kinds of damaged save, each rejected with its own message. A failed load keeps the game. The last row and column redraw after save and load. |
| `pty_test.py` (89 checks) | Arrow keys and `wasd`, placing, the computer's reply, saving, aborting with `q` and the statistics afterwards, empty names, corner redraws after loading and saving, loading an O-start game during an X-start game and the reverse, loading a won or full position, damaged saves in the slot list and while loading, every original save, closed input, and fixed versus different seeds. |

## What changed from `AMOEBA.C`

### Turns, results and statistics

- **One turn loop.** The game alternates X and O in a single loop, not in two branches picked by who started. A load therefore can't hand one side two moves in a row.
- **Loading resumes with X to move.** The original program could only save during your move, so every save, old or new, is a position where X moves next. After a load, the game first checks the restored position: if it already has five in a row, or the board is full, the game ends there without asking for a move. A loaded win is counted in the statistics and decides who starts next, like any other win.
- **Draws.** When the board is full, the game ends as `WINNER: - (draw)`. A win is always checked before a draw.
- **Aborting with `q`.** The game ends as `WINNER: - (aborted)`, and neither side gets a win. The original added a win for O. After an abort or a draw, the same side starts the next game. After a win, the loser starts, as before.
- **The statistics are shown right away.** `X WON` / `O WON` are redrawn as soon as a game ends.
- **ROUND counts completed rounds.** A round is one move by each side. The counter goes up when the second player of the round has moved, and the display updates after every move.
- **Opening move.** The computer plays its fixed opening square, M12, only on an empty board. The original used "round 0 and O starts", which after a load could fire on a board that already had stones.
- **Computer moves are always legal.** The computer only considers empty squares, including when every score is 0, so it can no longer overwrite a stone. `putx()` and `puto()` refuse an occupied or off-board square.

### Computer strategy

The greedy evaluation is the original one with the fixes below (§ *Scoring*). The function called `minimax()` is still a one-move evaluation. On top of it, the computer now looks ahead for forced wins (§ *Lookahead*). These are the changes to the evaluation:

- **Safe board reads.** Every line scan checks both coordinates before it reads a square, including the squares just past a run. The original read outside the board (index −1, or one past the last row or column).
- **The board edge blocks a line.** When a run reaches the edge, that end counts as blocked, just as if an opponent stone stood there. A run that has an empty square before the edge stays open. For example, `|XXX[c]_` now scores as a blocked four (5462), where the original scored it as an open four (21845).
- **Gap patterns are detected.** The original's gap check could never run. See the next section.
- **Every four with one winning square scores 5462.** That covers blocked fours, broken fours and blocked broken fours. Only an open four, with two winning squares, keeps 21845. The original weights put a blocked four (1365) below an open three (5461), so the computer missed winning combinations of fours and threes and failed to block them. See *How combinations rank* below.
- **Winner detection** checks the board it is given, not the global one.

### Single-gap patterns (strategy decisions)

For each of the 4 directions through an empty square, the computer reads the line in up to three ways and keeps the reading with the highest score:

1. **Unbroken run.** The run of own stones through the square, counting the new stone. Code 1–4, or 5 for five or more.
2. **Run + gap ahead.** The run, then exactly one empty square, then more own stones.
3. **Run + gap behind.** The same on the other side.

For a gap reading, add the stones on both sides of the gap:

| Stones on both sides of the gap | Code | Meaning | Examples (`c` is the square being scored) |
|---|---|---|---|
| 3 | 8 | broken three: filling the gap makes four | `cX_X`, `c_XX`, `XX_c` |
| 4 or more | 9 | broken four: filling the gap makes five | `cXX_X`, `cX_XX`, `XX_Xc` |
| 2 | – | `X_X` is not scored, as in the original | `X_c` |

- The scan past the gap stops at the first square that isn't an own stone. So an opponent, the board edge or a second gap ends it: `cX_X_X` is a broken three, not a four.
- A run that is already four long gets no gap reading; it is already a four.
- A gap pattern's blocked ends are the squares just outside the whole pattern: past the far end of the run, and past the stones after the gap.

These choices are deliberate and have been confirmed:

- **Code by total stones.** The original hinted at "run + 6", which would have scored `XX_XX` as a three. Counting all stones gives each pattern the category of what filling the gap produces.
- **Keep the best of the readings.** A four now scores one point more than an open three, so `_cXX_XO` counts as the blocked broken four it is, not as the open three it also looks like.
- **Score a free broken four like a blocked four.** A free broken four (`XXX_X` with both outer ends open) has only one winning square, the gap, so it scores 5462 like any other four that can be blocked with one stone. The original table gave it 21844, just below an open four (21845).

### Scoring

| Code | room ≥ 6, no blocked end | room ≥ 6, blocked | room = 5, no blocked end | room = 5, blocked |
|---|---|---|---|---|
| 5 (five) | win (`UINT_MAX`) | win | win | win |
| 4 (four) | 21845 | 5462 | *cannot occur* | 5462 |
| 9 (broken four) | 5462 | 5462 | *cannot occur* | 5462 |
| 3 (three) | 5461 | 341 | 341 | 341 |
| 8 (broken three) | 5460 | 340 | 340 | 340 |
| 2 (two) | 85 | 5 | 21 | 5 |

"Room" is the number of squares free of opponent stones within 4 squares of the square, along the line. With fewer than 5, nothing scores. A square's value is the sum over its 4 directions, and a five ends the sum at once with the win score.

The two *cannot occur* cells are left out on purpose. A four, or a broken four, with no blocked end has an empty square at each end, so it always has at least 6 squares of room. The exhaustive unit test checks this for every arrangement of the 8 nearby squares.

#### How combinations rank

Because a square's directions add up, the weight of a four decides how combinations rank. A four scores 5462, one point more than an open three (5461). On their own, the combinations rank in this order:

| Best pattern on the square | Score | With the original weights |
|---|---|---|
| Open four | 21845 | 21845 |
| Double four | 10924 | 2730 (two blocked fours) to 43688 (two broken fours) |
| Four-three | 10923 | 6826 (blocked four) to 27305 (broken four) |
| Double three | 10922 | 10922 |
| Four | 5462 | 1365 (blocked) to 21844 (broken) |
| Open three | 5461 | 5461 |

- **Most steps are one point.** A four-three whose three is broken scores 10922 and only ties a double three. Any lesser pattern in a spare direction, even a blocked two (5), can reverse a one-point step. For example, an open three with a blocked two across it outscores a lone four.
- **The step from two threats to one is wide.** A square with two open threes or fours scores at least 10920. A square with only one of them, other than an open four (21845), scores at most 6485: a four with a blocked three in each other direction.
- **Why 5462.** A four weight from 6485 to 9896 would keep every step in order, even with blocked threes in the spare directions. But 5462 played better in games between computer players. Over 2000 random four-stone openings, each played twice with the sides swapped, 5462 won 57–59% of the decided games against 8191. Against the original weights, 5462 won about 70% and 8191 about 62%. These figures hold with and without the random tie-breaking. In an earlier, smaller sweep, the weights 6485, 7300, 8191, 9000 and 9896 played identical games, but that holds only for those games: with three or four busy directions on one square, the weights cross other thresholds and can rank squares differently.

With the original weights, a four-three built on a blocked four scored less than a double three. So the computer would answer your double three when it could win with such a four-three. It would also make its own double three instead of blocking yours, and lose. The unit tests check this order, the tie, the gap between two threats and one, and both moves.

When the lookahead finds nothing forced, the computer compares the best square for itself with the best square for you. It plays your best square only if that scores strictly higher; otherwise it plays its own. Among equal squares, the first one in scan order is kept, and each later equal square replaces it with probability 1/5. That is the original tie rule, now with a seeded generator.

### Lookahead

The scores above only see what a single move makes, so a player who plans a double threat two moves ahead beats them most of the time. Before using the scores, the computer now searches for forced wins:

- **A four** leaves the other side one square to block. A run of fours that ends with two squares to make five is a forced win.
- **A three** is a move after which, moving again, the side would win by fours. The other side must answer it, by blocking or with a four of its own. A forced win may use up to 2 threes; each run of fours may be up to 20 long.

Each move, the computer does the first of these that applies:

1. If either side has a square that makes five, the scores already pick it: win, or block.
2. If it has a forced win, it plays the first move. It takes the win with the fewest threes it finds, so an attack heads for its end. When it took any win it found, it could switch to another, equally long win after every reply and never finish. The search is limited, so this is how it behaves in tests, not a proof that every attack ends.
3. If you have a forced win, it tries the squares of your winning line, the squares around its first move, and the squares where you make a three or a four, best score (yours plus its own) first. It plays the first one after which the search finds no forced win for you. On a square where it makes a four, the check first plays out the forced replies in turn: you block its four; if your block makes a four of yours, it blocks that; and so on. Then it searches your move. If your block leaves it a free move, the square counts as safe only if you would have no forced win even if it passed. A check that runs out of steps proves nothing, so that square doesn't count as safe; it is played only if no square is found safe.
4. Otherwise it uses the scores, as before.

Each search is limited to 40,000 steps. The checks of possible blocks share 400,000 steps per move, on top of its two searches for forced wins (up to 3 × 40,000 steps each). A step is one position searched, and a little work continues after a limit is reached, so these limits bound the time only roughly; there is no clock limit. A move takes about 9–11 ms on average; the slowest seen was 375 ms, in 1000-game test runs with 8 games running at once. The search does not see a quiet move that prepares a fork; it blocks the fork once it is one move away. A move that prepares two forks at once can still beat it.

To check that a forced win the search reports is really forced, the unit tests play it out on random positions against every reply that could matter, with a fresh search for each of the computer's moves. In 150 positions, all 107 wins held, 48 of them using threes. Over about 1900 claims in longer runs, wins by fours alone always held; the few that could not be confirmed were all cases where a later search ran out of steps, not refutations.

How much harder it got, from an empty board with the random tie-breaking on, 400 games per row. The planner is a test opponent that looks 3 or 4 of its own moves ahead for a forced win and knows exactly how the computer will reply, like a player who has learned its habits; otherwise it plays like the version it faces.

| Computer | Planner wins (3 moves ahead) | Planner wins (4 moves ahead) |
|---|---|---|
| Original `AMOEBA.C` | 60.5% | 82% |
| Greedy scores only (four weight 5462) | 57% | 80% |
| With lookahead | 46% | 45% |

In games between computer players, the lookahead version won 88% of the decided games against the original and 82% against the greedy version (1000 games each). Against the lookahead as it was before the code review's fixes, it won 508–463 with 29 draws: no clear difference.

### Saving and loading

- **The file format is unchanged.** A save holds the name, then `game`, `round`, `X won`, `O won`, `starting player` (1 = X, 2 = O), each on its own line. Then comes one `[x,y]=p` line per stone, with 0-based column `x` (0–23), row `y` (0–22) and player `p` (1 or 2). Old saves load, and the original program can read new saves (except ones with an empty name, which it misreads, as it did its own). No turn field was added, because the side to move is always X (see above).
- **Strict, line-based parsing.** A save is read into a temporary copy and checked completely before it replaces the running game. If anything is wrong, you see `saveN line L: <problem> (press enter)` and your current game carries on unchanged. Rejected:
  - an empty file, or a missing header line;
  - a name longer than 8 letters, or with characters other than `a`–`z`;
  - counters that aren't plain decimal numbers in 0 … 1,000,000,000;
  - a starting player other than 1 or 2;
  - stone lines with any other shape, trailing text, or numbers too large to convert;
  - squares off the board, players other than 1 or 2, a square listed twice;
  - lines longer than 62 characters.

  Blank lines, Windows line endings and a missing final newline are accepted; the old loader tolerated blank lines too.
- **Empty names.** Pressing Enter right away saves the name as an empty line. It loads correctly and shows as `(no name)` in the slot list. The original loader read the next line as the name, which misaligned everything after it.
- **Slot list.** Each slot shows its name, `(no name)`, `(damaged)` for a file that wouldn't load, or nothing if it is empty.
- **Messages.** The debug lines that flashed past while loading are gone. When a save can't be written, the message now names the file, not the save name.

### Screen

- After a save or a load, the board redraw uses the right dimensions. Stones in the last column (column X) are drawn again, and nothing is drawn on row 24. The original swapped the two limits.

### Terminal layer (`conio_fixed.h`)

- `getch()` and `getche()` return the key they read. In the original they fell off the end of the function, which is undefined behaviour. With Apple clang on arm64 the game got no real key input at all.
- `getche()` now really turns echo on: `|= ECHO` where the original had `&= ECHO`. The game doesn't use it.
- `wherexy()`, `getpass()` and `gettext()` now return a value, so the header compiles without warnings. The game uses none of them.

## Limitations

- Tested only on macOS (Apple clang 21, arm64). Not built with GCC or tested on Linux.
- The pseudo-terminal tests send real key sequences to the real program, but no person has played a full game interactively with this version.
- The lookahead only follows forcing moves (threes and fours). It can't see a quiet move that prepares a fork, and a move that prepares two forks at once can still beat it. Its search is limited in steps, so in a complicated position it can miss a forced win or a defence.
- Saving writes over the slot file directly. If the program dies mid-write, the file may be damaged; it will then show as `(damaged)` and won't load.
- Name entry has no backspace, and the slot prompts can't be cancelled (as in the original).
- As in the original `conio.h`, the terminal goes back to normal echo mode between key reads. Keys pressed while the computer is thinking may briefly appear on screen.
- `conio_2.h`, the ncurses version, still isn't used and doesn't fit this program.

#!/usr/bin/env python3
"""End-to-end tests: play the real game binary through a pseudo-terminal.

Usage: python3 tests/pty_test.py path/to/amoeba [folder with the original save1..save5]

Every test runs the game in its own temporary folder, so the project's save files are
only ever copied, never written.
"""
import fcntl
import os
import pty
import re
import select
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time

ESC = '\x1b'
UP, DOWN, RIGHT, LEFT = ESC + '[A', ESC + '[B', ESC + '[C', ESC + '[D'
TOP_LEFT = LEFT * 24 + UP * 23          # walks the cursor to A1 from anywhere
STONE = re.compile(r'\x1b\[(\d+);(\d+)f\x1b\[1;3([14]);40m([XO])')

failures = 0


def check(cond, what):
    global failures
    if not cond:
        failures += 1
    print('  %s %s' % ('ok  ' if cond else 'FAIL', what))


class Game:
    def __init__(self, binary, cwd, seed='7'):
        env = dict(os.environ, AMOEBA_SEED=seed, TERM='xterm')
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            try:
                os.chdir(cwd)
                os.execve(binary, [binary], env)
            finally:
                os._exit(127)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack('HHHH', 25, 80, 0, 0))
        self.log = ''
        self.screen = self.read()

    def read(self, idle=0.2, timeout=10, first=5):
        """Waits up to `first` seconds for output (every key the tests send produces some),
        then collects it until the game has been quiet for `idle` seconds."""
        data = b''
        end = time.time() + timeout
        while time.time() < end:
            ready, _, _ = select.select([self.fd], [], [], idle if data else first)
            if not ready:
                break
            try:
                chunk = os.read(self.fd, 65536)
            except OSError:
                break
            if not chunk:
                break
            data += chunk
        text = data.decode('latin-1')
        self.log += text
        return text

    def send(self, keys):
        os.write(self.fd, keys.encode('latin-1'))
        return self.read()

    def finish(self, timeout=5):
        """Waits for the game to exit and returns its exit status."""
        end = time.time() + timeout
        status = None
        while time.time() < end:
            pid, raw = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                status = os.waitstatus_to_exitcode(raw)
                break
            self.read(idle=0.05, timeout=0.2, first=0.05)
        if status is None:
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
        os.close(self.fd)
        check('runtime error' not in self.log and 'Sanitizer' not in self.log,
              'no sanitizer report in the game output')
        return status


def drawn(out):
    """Stones drawn in a chunk of output, as (x, y, player) with 0-based x, y."""
    return [(int(col) // 2 - 1, int(row) - 1, 1 if c == 'X' else 2)
            for row, col, _, c in STONE.findall(out)]


def save_text(name, game, rnd, x_won, o_won, start, stones):
    lines = [name, str(game), str(rnd), str(x_won), str(o_won), str(start)]
    lines += ['[%d,%d]=%d' % s for s in sorted(stones)]
    return '\n'.join(lines) + '\n'


def write(folder, name, text):
    with open(os.path.join(folder, name), 'w') as f:
        f.write(text)


def read_save(folder, name):
    with open(os.path.join(folder, name)) as f:
        lines = f.read().split('\n')
    stones = [tuple(int(v) for v in re.fullmatch(r'\[(\d+),(\d+)\]=(\d)', r).groups())
              for r in lines[6:-1]]
    return dict(name=lines[0], game=int(lines[1]), round=int(lines[2]), x_won=int(lines[3]),
                o_won=int(lines[4]), start=int(lines[5]), stones=stones,
                xs=sum(1 for s in stones if s[2] == 1), os=sum(1 for s in stones if s[2] == 2))


def save_as(g, slot, name):
    g.send('o')
    g.send(str(slot))
    return g.send(name + '\r')


def load(g, slot):
    g.send('l')
    return g.send(str(slot))


def quit_game(g):
    g.send('q')
    g.send('n')
    check(g.finish() == 0, 'q then n exits with status 0')


O_START = [(12, 11, 2), (11, 10, 1), (13, 12, 2)]                 # X to move, O started
X_START = [(5, 5, 1), (6, 6, 2), (7, 5, 1), (8, 8, 2)]           # X to move, X started


def test_keyboard_save_and_quit(binary, d):
    print('keyboard, placing, saving, quitting')
    g = Game(binary, d)
    check('MENU' in g.screen and 'GAME:    1.' in g.screen, 'the board and menu are drawn')
    for keys, pos, what in [(RIGHT, '11;28', 'right arrow'), (RIGHT, '11;30', 'right arrow'),
                            (DOWN, '12;30', 'down arrow'), (UP, '11;30', 'up arrow'),
                            (LEFT, '11;28', 'left arrow'), ('d', '11;30', 'd'), ('s', '12;30', 's'),
                            ('a', '12;28', 'a'), ('w', '11;28', 'w'), ('d', '11;30', 'd'),
                            ('s', '12;30', 's')]:
        out = g.send(keys)
        check(out.endswith('\x1b[%sf' % pos), '%s moves the cursor' % what)
    out = g.send(' ')
    stones = drawn(out)
    check(stones[:1] == [(14, 11, 1)], 'space places X under the cursor (O12)')
    check(len(stones) == 2 and stones[1][2] == 2, 'the computer answers with one O')
    check('ROUND:   1.' in out, 'one round completed')
    out = g.send(' ')
    check(drawn(out) == [], 'space on an occupied square does nothing')
    save_as(g, 1, 'abc')
    s = read_save(d, 'save1')
    check((s['name'], s['game'], s['round'], s['x_won'], s['o_won'], s['start']) == ('abc', 1, 1, 0, 0, 1),
          'save header: name, game, round, wins, starter')
    check(sorted(s['stones']) == sorted(stones), 'save holds both stones')
    out = g.send('q')
    check('WINNER: - (aborted)' in out, 'q shows the game as aborted')
    check('X WON:   0' in out and 'O WON:   0' in out, 'q gives neither side a win')
    out = g.send('y')
    check('GAME:    2.' in out, 'y starts game 2')
    check(drawn(out) == [], 'after an abort X still starts: the computer does not move first')
    save_as(g, 2, '')
    s = read_save(d, 'save2')
    check((s['name'], s['game'], s['x_won'], s['o_won'], s['start'], s['stones']) == ('', 2, 0, 0, 1, []),
          'an empty name saves as an empty line; counters unchanged')
    out = g.send('l')
    check('2.:(no name)' in out, 'the slot list shows unnamed saves')
    out = g.send('2')
    check('GAME:    2.' in out, 'the unnamed save loads')
    quit_game(g)


def test_corner_redraw(binary, d):
    print('redrawing the last row and column')
    corners = [(0, 0, 1), (23, 0, 2), (0, 22, 1), (23, 22, 2)]
    write(d, 'save1', save_text('corner', 1, 2, 0, 0, 1, corners))
    g = Game(binary, d)
    out = load(g, 1)
    check(sorted(drawn(out)) == sorted(corners), 'loading redraws all four corners')
    check(not re.search(r'\x1b\[24;\d+f\x1b\[1;3[14];40m[XO]', out), 'nothing is drawn below row 23')
    out = save_as(g, 2, 'copy')
    check(sorted(drawn(out)) == sorted(corners), 'saving redraws all four corners')
    s = read_save(d, 'save2')
    check(s['name'] == 'copy' and sorted(s['stones']) == sorted(corners), 'the save holds the corners')
    quit_game(g)


def test_load_o_start_during_x_game(binary, d):
    print('loading an O-start game during an X-start game')
    write(d, 'save1', save_text('ostart', 4, 1, 1, 2, 2, O_START))
    g = Game(binary, d)
    load(g, 1)
    g.send(TOP_LEFT)
    stones = drawn(g.send(' '))
    check(stones[:1] == [(0, 0, 1)] and len(stones) == 2 and stones[1][2] == 2,
          'X moves, then the computer moves exactly once')
    save_as(g, 2, 'after')
    s = read_save(d, 'save2')
    check((s['start'], s['game'], s['round'], s['x_won'], s['o_won'], s['xs'], s['os']) == (2, 4, 2, 1, 2, 2, 3),
          'restored game continues: O started, round 2, 2 X and 3 O')
    quit_game(g)


def test_load_x_start_during_o_game(binary, d):
    print('loading an X-start game during an O-start game')
    write(d, 'save1', save_text('ostart', 4, 1, 1, 2, 2, O_START))
    write(d, 'save3', save_text('xstart', 2, 2, 0, 1, 1, X_START))
    g = Game(binary, d)
    load(g, 1)
    g.send(TOP_LEFT)
    g.send(' ')                       # now inside an O-start game, after the computer moved
    load(g, 3)
    g.send(TOP_LEFT)
    stones = drawn(g.send(' '))
    check(stones[:1] == [(0, 0, 1)] and len(stones) == 2 and stones[1][2] == 2,
          'X moves, then the computer moves exactly once')
    save_as(g, 4, 'after')
    s = read_save(d, 'save4')
    check((s['start'], s['game'], s['round'], s['xs'], s['os']) == (1, 2, 3, 3, 3),
          'restored game continues: X started, round 3, 3 X and 3 O')
    quit_game(g)


def test_finished_positions(binary, d):
    print('loading positions that are already over')
    won = [(3 + i, 3, 1) for i in range(5)] + [(3, 5, 2), (4, 5, 2), (5, 5, 2), (6, 5, 2), (8, 8, 2)]
    write(d, 'save1', save_text('won', 1, 4, 0, 0, 1, won))
    full = [(x, y, 1 if (x + 2 * y) % 4 < 2 else 2) for x in range(24) for y in range(23)]
    write(d, 'save2', save_text('full', 1, 276, 0, 0, 1, full))
    g = Game(binary, d)
    out = load(g, 1)
    check('WINNER: \x1b[1;34;40mX' in out and 'NEW GAME?(y/n)' in out, 'a won position ends the game at once')
    check(len(drawn(out)) == len(won), 'no further move is played')
    check('X WON:   1' in out, 'the win is counted')
    out = g.send('y')
    out = load(g, 2)
    check('WINNER: - (draw)' in out and 'NEW GAME?(y/n)' in out, 'a full board ends as a draw')
    check('X WON:   0' in out and 'O WON:   0' in out, 'a draw gives neither side a win')
    g.send('n')
    check(g.finish() == 0, 'n exits with status 0')


def test_bad_save_keeps_game(binary, d):
    print('a damaged save leaves the current game alone')
    good = [(10, 10, 1), (11, 11, 2)]
    write(d, 'save1', save_text('good', 1, 1, 0, 0, 1, good))
    write(d, 'save4', 'z' * 300 + '\n1\n1\n0\n0\n1\n')
    write(d, 'save5', save_text('bad', 1, 1, 0, 0, 1, []) + '[1,2=1\n')
    g = Game(binary, d)
    load(g, 1)
    out = g.send('l')
    check('1.:good' in out and '4.:(damaged)' in out and '5.:(damaged)' in out,
          'the slot list marks damaged saves')
    out = g.send('5')
    check('save5 line 7: stone must look like [x,y]=1 (press enter)' in out, 'the error names file and line')
    out = g.send('\r')
    check(sorted(drawn(out)) == sorted(good), 'the current game is redrawn unchanged')
    save_as(g, 2, 'kept')
    s = read_save(d, 'save2')
    check(sorted(s['stones']) == sorted(good) and s['round'] == 1, 'saving afterwards writes the kept game')
    quit_game(g)


def test_legacy_saves(binary, d, legacy):
    print('the original save files')
    for slot in range(1, 6):
        src = os.path.join(legacy, 'save%d' % slot)
        if not os.path.exists(src):
            print('  (skipping %s: not found)' % src)
            continue
        shutil.copy(src, d)
        expect = read_save(d, 'save%d' % slot)
        g = Game(binary, d)
        out = load(g, slot)
        check(sorted(drawn(out)) == sorted(expect['stones']) and 'GAME:  %3d.' % expect['game'] in out,
              'save%d loads and redraws its %d stones' % (slot, len(expect['stones'])))
        quit_game(g)
        with open(src, 'rb') as a, open(os.path.join(d, 'save%d' % slot), 'rb') as b:
            check(a.read() == b.read(), 'save%d is unchanged' % slot)


def test_seeding(binary):
    print('seeding the tie-breaks')
    replies = set()
    for seed in ['99', '99'] + [str(s) for s in range(1, 11)]:
        d = tempfile.mkdtemp(prefix='amoeba-pty-')
        g = Game(binary, d, seed)
        reply = tuple(drawn(g.send(' ')))
        if seed == '99':
            replies.add(reply)
        else:
            replies.add(('other', reply))
        g.send('q')
        g.send('n')
        g.finish()
        shutil.rmtree(d)
    same = [r for r in replies if r[0] != 'other']
    check(len(same) == 1, 'the same AMOEBA_SEED gives the same reply')
    check(len({r[1] for r in replies if r[0] == 'other'}) > 1, 'different seeds give different tie-breaks')


def test_closed_input(binary, d):
    print('closed input')
    try:
        r = subprocess.run([binary], cwd=d, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE, timeout=10)
        check(r.returncode == 1 and b'Sanitizer' not in r.stderr, 'the game exits with status 1 at end of input')
    except subprocess.TimeoutExpired:
        check(False, 'the game exits at end of input instead of spinning')


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    binary = os.path.abspath(sys.argv[1])
    legacy = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else None
    tests = [test_keyboard_save_and_quit, test_corner_redraw, test_load_o_start_during_x_game,
             test_load_x_start_during_o_game, test_finished_positions, test_bad_save_keeps_game,
             test_closed_input]
    for t in tests:
        d = tempfile.mkdtemp(prefix='amoeba-pty-')
        try:
            t(binary, d)
        finally:
            shutil.rmtree(d)
    if legacy:
        d = tempfile.mkdtemp(prefix='amoeba-pty-')
        try:
            test_legacy_saves(binary, d, legacy)
        finally:
            shutil.rmtree(d)
    test_seeding(binary)
    print('%s' % ('all passed' if not failures else '%d failure(s)' % failures))
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()

# timekeeper

`timekeeper` is a small command-line utility written in C for encoding and decoding
dates and times using the Arvelie date format and Neralie time format.

It supports:

- Gregorian date → Arvelie date
- Arvelie date → Gregorian date
- Standard time → Neralie time
- Neralie time → Standard time
- Optional custom epoch / year-zero date
- Configurable default epoch via environment variable or config file

## Examples

```sh
timekeeper encode-date 2024-1-18
# 2024B04

timekeeper decode-date 2024B04
# 2024-01-18

timekeeper encode-time 06:00:00
# 250:000

timekeeper decode-time 250:000
# 06:00:00
```

## Arvelie date format

Arvelie divides the year into 26 months of 14 days each.

The months are represented by letters:

```txt
A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
```

A normal Arvelie date looks like this:

```txt
YYYYA01
YYYYB04
YYYYZ14
```

The remaining day at the end of a normal year is represented with `+00`.

In a leap year, the final extra day is represented with `+01`.

Examples:

```sh
timekeeper encode-date 2023-12-31
# 2023+00

timekeeper encode-date 2024-12-31
# 2024+01
```

## Neralie time format

Neralie divides a day into 1000 beats, with each beat divided into 1000 pulses.

A Neralie time looks like this:

```txt
BBB:PPP
```

Examples:

```sh
timekeeper encode-time 00:00:00
# 000:000

timekeeper encode-time 06:00:00
# 250:000

timekeeper encode-time 12:00:00
# 500:000
```

## Custom epoch support

One of the useful features of Arvelie dates is that year zero can be set to an
arbitrary date.

For example, with an epoch of `1990-05-12`:

```sh
timekeeper --epoch 1990-05-12 encode-date 1990-05-12
# 0000A01

timekeeper --epoch 1990-05-12 encode-date 1990-05-25
# 0000A14

timekeeper --epoch 1990-05-12 encode-date 1990-05-26
# 0000B01

timekeeper --epoch 1990-05-12 decode-date 0000A01
# 1990-05-12
```

When an epoch is configured, Arvelie year `0000` begins on the epoch date.

### Epoch source priority

Epoch configuration is loaded in this order:

```txt
1. --epoch YYYY-MM-DD or --epoch=YYYY-MM-DD
2. TIMEKEEPER_EPOCH=YYYY-MM-DD
3. $XDG_CONFIG_HOME/timekeeper/config
4. ~/.config/timekeeper/config
```

The command-line option has the highest priority.

### One-off epoch

```sh
timekeeper --epoch 1990-05-12 encode-date 2024-01-18
```

You may also use the equals form:

```sh
timekeeper --epoch=1990-05-12 encode-date 2024-01-18
```

### Environment variable

```sh
export TIMEKEEPER_EPOCH=1990-05-12

timekeeper encode-date 1990-05-12
# 0000A01
```

### Config file

Create a config file at:

```txt
$XDG_CONFIG_HOME/timekeeper/config
```

or, if `XDG_CONFIG_HOME` is not set:

```txt
~/.config/timekeeper/config
```

Example:

```sh
mkdir -p ~/.config/timekeeper
printf "epoch=1990-05-12\n" > ~/.config/timekeeper/config
```

Config file format:

```txt
epoch=1990-05-12
```

### Disable epoch for one command

If you have a configured epoch but want to use the normal Gregorian year for one
command, use `--no-epoch`:

```sh
timekeeper --no-epoch encode-date 1990-05-12
# 1990J06
```

## Usage

```txt
usage:
  timekeeper [--epoch YYYY-MM-DD] encode-date YYYY-M-D
  timekeeper [--epoch YYYY-MM-DD] decode-date YYYY[A-Z|+]DD
  timekeeper encode-time HH:MM:SS
  timekeeper decode-time BBB:PPP
```

Commands:

```txt
encode-date   Convert Gregorian date to Arvelie date
decode-date   Convert Arvelie date to Gregorian date
encode-time   Convert standard HH:MM:SS time to Neralie time
decode-time   Convert Neralie BBB:PPP time to standard time
```

Options:

```txt
--epoch YYYY-MM-DD    Use a custom year-zero date
--epoch=YYYY-MM-DD   Same as above
--no-epoch           Disable configured epoch for one command
```

## Building from source

### Requirements

You need:

- A C compiler, such as `gcc`
- `make`

On Debian/Ubuntu:

```sh
sudo apt update
sudo apt install build-essential
```

On Fedora:

```sh
sudo dnf install gcc make
```

On Arch Linux:

```sh
sudo pacman -S base-devel
```

### Build

```sh
make
```

This creates the `timekeeper` binary:

```sh
./timekeeper encode-date 2024-1-18
```

### Run tests

```sh
make test
```

### Clean build artifacts

```sh
make clean
```

## Installing on Linux

To install `timekeeper` so it can be run from anywhere in the terminal, copy the
compiled binary into a directory on your `PATH`.

### System-wide install

Build the program:

```sh
make
```

Install it to `/usr/local/bin`:

```sh
sudo install -Dm755 timekeeper /usr/local/bin/timekeeper
```

Now you can run:

```sh
timekeeper encode-date 2024-1-18
```

### User-local install

If you do not want to use `sudo`, install it into `~/.local/bin`:

```sh
make
mkdir -p ~/.local/bin
install -Dm755 timekeeper ~/.local/bin/timekeeper
```

Make sure `~/.local/bin` is on your `PATH`.

For Bash, add this to `~/.bashrc` if needed:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

For Zsh, add this to `~/.zshrc` if needed:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

For Fish, run:

```fish
fish_add_path ~/.local/bin
```

Then restart your shell or reload your config.

Verify installation:

```sh
which timekeeper
timekeeper encode-date 2024-1-18
```

## Uninstalling

If installed system-wide:

```sh
sudo rm -f /usr/local/bin/timekeeper
```

If installed user-local:

```sh
rm -f ~/.local/bin/timekeeper
```

## Development

A typical development loop:

```sh
make clean
make
make test
```

For a smaller release binary, if your `Makefile` supports the `size` target:

```sh
make clean
make size
make test
```

For sanitizer/debug builds, if your `Makefile` supports the `debug` target:

```sh
make clean
make debug
make test
```

## Notes

- Date parsing is intentionally strict.
- Years are limited to the supported range of the program.
- Invalid dates, times, or malformed Arvelie/Neralie values return a non-zero
  exit code.
- Epoch-aware `decode-date` must use the same epoch that was used for
  `encode-date`, otherwise the decoded Gregorian date will be different.

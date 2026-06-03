/*
 * timekeeper - A command-line utility for encoding and decoding
 * dates and times using the Arvelie date and Neralie time format.
 * Copyright (C) 2026 M. Peterson
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with 
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>

enum {
    MIN_YEAR = 1,
    MAX_YEAR = 9999,
    MIN_ARVELIE_YEAR = 0,
    MAX_ARVELIE_YEAR = 9999,
    CONFIG_PATH_MAX = 4096,
    CONFIG_LINE_MAX = 256
};

typedef struct {
    int year;
    int month;
    int day;
} Date;

typedef struct {
    Date epoch;
    int has_epoch;
} ArvelieConfig;

static const int DAYS_BEFORE_MONTH_COMMON[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static const int DAYS_BEFORE_MONTH_LEAP[12] = {
    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335
};

static int parse_uint_bounded(
    const char **p,
    int min,
    int max,
    int *out
) {
    const char *s = *p;
    long value;
    char *end = NULL;

    if (*s == '\0' || !isdigit((unsigned char)*s)) {
        return 0;
    }

    errno = 0;
    value = strtol(s, &end, 10);

    if (errno == ERANGE || end == s) {
        return 0;
    }

    if (value < min || value > max) {
        return 0;
    }

    *out = (int)value;
    *p = end;
    return 1;
}

static int expect_char(const char **p, char c) {
    if (**p != c) {
        return 0;
    }

    (*p)++;
    return 1;
}

static int expect_end(const char *p) {
    return *p == '\0';
}

static int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_month(int year, int month) {
    static const int days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month < 1 || month > 12) {
        return 0;
    }

    if (month == 2 && is_leap_year(year)) {
        return 29;
    }

    return days[month - 1];
}

static int is_valid_date(int year, int month, int day) {
    if (year < MIN_YEAR || year > MAX_YEAR) {
        return 0;
    }

    if (month < 1 || month > 12) {
        return 0;
    }

    return day >= 1 && day <= days_in_month(year, month);
}

static int day_of_year(int year, int month, int day) {
    const int *days_before;

    if (!is_valid_date(year, month, day)) {
        return -1;
    }

    days_before = is_leap_year(year)
        ? DAYS_BEFORE_MONTH_LEAP
        : DAYS_BEFORE_MONTH_COMMON;

    return days_before[month - 1] + day;
}

static int date_from_day_of_year(int year, int yday, int *month, int *day) {
    int max_yday;

    if (year < MIN_YEAR || year > MAX_YEAR) {
        return 0;
    }

    max_yday = is_leap_year(year) ? 366 : 365;

    if (yday < 1 || yday > max_yday) {
        return 0;
    }

    for (int m = 1; m <= 12; m++) {
        int dim = days_in_month(year, m);

        if (yday <= dim) {
            *month = m;
            *day = yday;
            return 1;
        }

        yday -= dim;
    }

    return 0;
}

/*
    Days since 0000-03-01, adapted from Howard Hinnant's civil calendar
    algorithm. This is used only for safe date differences/addition; user-facing
    Gregorian years are still restricted to 0001..9999.
*/
static int64_t days_from_civil(int year, int month, int day) {
    int y = year;
    unsigned m = (unsigned)month;
    unsigned d = (unsigned)day;
    int era;
    unsigned yoe;
    unsigned doy;
    unsigned doe;

    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153U * (m + (m > 2 ? (unsigned)-3 : 9U)) + 2U) / 5U + d - 1U;
    doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;

    return (int64_t)era * 146097LL + (int64_t)doe;
}

static int civil_from_days(int64_t z, Date *out) {
    int64_t era = (z >= 0 ? z : z - 146096LL) / 146097LL;
    unsigned doe = (unsigned)(z - era * 146097LL);
    unsigned yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
    int y = (int)yoe + (int)era * 400;
    unsigned doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
    unsigned mp = (5U * doy + 2U) / 153U;
    unsigned d = doy - (153U * mp + 2U) / 5U + 1U;
    unsigned m = mp + (mp < 10U ? 3U : (unsigned)-9);
    int year = y + (m <= 2U);

    if (!is_valid_date(year, (int)m, (int)d)) {
        return 0;
    }

    out->year = year;
    out->month = (int)m;
    out->day = (int)d;
    return 1;
}

static int compare_date(Date a, Date b) {
    if (a.year != b.year) {
        return a.year < b.year ? -1 : 1;
    }

    if (a.month != b.month) {
        return a.month < b.month ? -1 : 1;
    }

    if (a.day != b.day) {
        return a.day < b.day ? -1 : 1;
    }

    return 0;
}

static int anniversary_date(Date epoch, int arvelie_year, Date *out) {
    int target_year = epoch.year + arvelie_year;
    int target_day;

    if (arvelie_year < MIN_ARVELIE_YEAR || arvelie_year > MAX_ARVELIE_YEAR) {
        return 0;
    }

    if (target_year < MIN_YEAR || target_year > MAX_YEAR) {
        return 0;
    }

    target_day = epoch.day;

    /*
        If the epoch is Feb 29, non-leap-year anniversaries are clamped to
        Feb 28. This keeps every configured epoch valid without introducing an
        impossible Gregorian date.
    */
    if (target_day > days_in_month(target_year, epoch.month)) {
        target_day = days_in_month(target_year, epoch.month);
    }

    out->year = target_year;
    out->month = epoch.month;
    out->day = target_day;
    return 1;
}

static int parse_date(const char *s, int *year, int *month, int *day) {
    const char *p = s;

    if (!parse_uint_bounded(&p, MIN_YEAR, MAX_YEAR, year)) {
        return 0;
    }

    if (!expect_char(&p, '-')) {
        return 0;
    }

    if (!parse_uint_bounded(&p, 1, 12, month)) {
        return 0;
    }

    if (!expect_char(&p, '-')) {
        return 0;
    }

    if (!parse_uint_bounded(&p, 1, 31, day)) {
        return 0;
    }

    if (!expect_end(p)) {
        return 0;
    }

    return *day <= days_in_month(*year, *month);
}

static int parse_date_value(const char *s, Date *date) {
    return parse_date(s, &date->year, &date->month, &date->day);
}

static int parse_time_hms(const char *s, int *hour, int *min, int *sec) {
    const char *p = s;

    if (!parse_uint_bounded(&p, 0, 23, hour)) {
        return 0;
    }

    if (!expect_char(&p, ':')) {
        return 0;
    }

    if (!parse_uint_bounded(&p, 0, 59, min)) {
        return 0;
    }

    if (!expect_char(&p, ':')) {
        return 0;
    }

    if (!parse_uint_bounded(&p, 0, 59, sec)) {
        return 0;
    }

    return expect_end(p);
}

static int parse_neralie(const char *s, int *beats, int *pulses) {
    const char *p = s;

    if (!parse_uint_bounded(&p, 0, 999, beats)) {
        return 0;
    }

    if (!expect_char(&p, ':')) {
        return 0;
    }

    if (!parse_uint_bounded(&p, 0, 999, pulses)) {
        return 0;
    }

    return expect_end(p);
}

static int parse_arvelie_date(
    const char *s,
    int min_year,
    int *year,
    char *month_char,
    int *arvelie_day
) {
    const char *p = s;

    if (!parse_uint_bounded(&p, min_year, MAX_ARVELIE_YEAR, year)) {
        return 0;
    }

    if (*p == '\0') {
        return 0;
    }

    *month_char = *p;
    p++;

    if (*month_char != '+' && (*month_char < 'A' || *month_char > 'Z')) {
        return 0;
    }

    if (!parse_uint_bounded(&p, 0, 99, arvelie_day)) {
        return 0;
    }

    return expect_end(p);
}

static int arvelie_day_to_yday(char month_char, int arvelie_day, int *yday) {
    if (month_char == '+') {
        if (arvelie_day > 1) {
            return 0;
        }

        *yday = 365 + arvelie_day;
        return 1;
    }

    if (arvelie_day < 1 || arvelie_day > 14) {
        return 0;
    }

    *yday = (month_char - 'A') * 14 + arvelie_day;
    return 1;
}

static int print_arvelie_date(int arvelie_year, int yday, int max_yday) {
    if (arvelie_year < MIN_ARVELIE_YEAR || arvelie_year > MAX_ARVELIE_YEAR) {
        fprintf(stderr, "arvelie year out of range: %d\n", arvelie_year);
        return 1;
    }

    if (yday < 1 || yday > max_yday || yday > 366) {
        fprintf(stderr, "arvelie day-of-year out of range: %d\n", yday);
        return 1;
    }

    if (yday < 365) {
        int pday = yday % 14;
        int arvelie_month;
        char month_char;

        if (pday == 0) {
            pday = 14;
        }

        arvelie_month = (yday + 13) / 14;

        if (arvelie_month < 1 || arvelie_month > 26) {
            fprintf(stderr, "internal arvelie month out of range\n");
            return 1;
        }

        month_char = (char)('A' + arvelie_month - 1);

        printf("%04d%c%02d\n", arvelie_year, month_char, pday);
        return 0;
    }

    printf("%04d+%02d\n", arvelie_year, yday - 365);
    return 0;
}

static int encode_date_legacy(const char *date_str) {
    Date date;
    int yday;

    if (!parse_date_value(date_str, &date)) {
        fprintf(stderr, "invalid date: %s\n", date_str);
        return 1;
    }

    yday = day_of_year(date.year, date.month, date.day);

    if (yday < 0) {
        fprintf(stderr, "invalid date: %s\n", date_str);
        return 1;
    }

    return print_arvelie_date(date.year, yday, is_leap_year(date.year) ? 366 : 365);
}

static int decode_date_legacy(const char *date_str) {
    int year;
    char month_char;
    int arvelie_day;
    int yday;
    int month;
    int day;
    int max_yday;

    if (!parse_arvelie_date(date_str, MIN_YEAR, &year, &month_char, &arvelie_day)) {
        fprintf(stderr, "invalid arvelie date: %s\n", date_str);
        return 1;
    }

    if (!arvelie_day_to_yday(month_char, arvelie_day, &yday)) {
        fprintf(stderr, "invalid arvelie day: %d\n", arvelie_day);
        return 1;
    }

    max_yday = is_leap_year(year) ? 366 : 365;

    if (yday > max_yday) {
        fprintf(stderr, "invalid day-of-year %d for year %d\n", yday, year);
        return 1;
    }

    if (!date_from_day_of_year(year, yday, &month, &day)) {
        fprintf(stderr, "invalid day-of-year %d for year %d\n", yday, year);
        return 1;
    }

    printf("%04d-%02d-%02d\n", year, month, day);
    return 0;
}

static int encode_date_epoch(const char *date_str, const ArvelieConfig *config) {
    Date date;
    Date start;
    Date next;
    int arvelie_year;
    int64_t date_days;
    int64_t start_days;
    int64_t next_days;
    int yday;
    int max_yday;

    if (!parse_date_value(date_str, &date)) {
        fprintf(stderr, "invalid date: %s\n", date_str);
        return 1;
    }

    if (compare_date(date, config->epoch) < 0) {
        fprintf(stderr, "date is before configured epoch\n");
        return 1;
    }

    arvelie_year = date.year - config->epoch.year;

    if (!anniversary_date(config->epoch, arvelie_year, &start)) {
        fprintf(stderr, "arvelie year is outside supported range\n");
        return 1;
    }

    if (compare_date(date, start) < 0) {
        arvelie_year--;

        if (!anniversary_date(config->epoch, arvelie_year, &start)) {
            fprintf(stderr, "arvelie year is outside supported range\n");
            return 1;
        }
    }

    if (!anniversary_date(config->epoch, arvelie_year + 1, &next)) {
        fprintf(stderr, "next epoch anniversary is outside supported range\n");
        return 1;
    }

    date_days = days_from_civil(date.year, date.month, date.day);
    start_days = days_from_civil(start.year, start.month, start.day);
    next_days = days_from_civil(next.year, next.month, next.day);

    yday = (int)(date_days - start_days) + 1;
    max_yday = (int)(next_days - start_days);

    return print_arvelie_date(arvelie_year, yday, max_yday);
}

static int decode_date_epoch(const char *date_str, const ArvelieConfig *config) {
    int arvelie_year;
    char month_char;
    int arvelie_day;
    int yday;
    Date start;
    Date next;
    Date result;
    int64_t start_days;
    int64_t next_days;
    int64_t result_days;
    int max_yday;

    if (!parse_arvelie_date(
            date_str,
            MIN_ARVELIE_YEAR,
            &arvelie_year,
            &month_char,
            &arvelie_day
        )) {
        fprintf(stderr, "invalid arvelie date: %s\n", date_str);
        return 1;
    }

    if (!arvelie_day_to_yday(month_char, arvelie_day, &yday)) {
        fprintf(stderr, "invalid arvelie day: %d\n", arvelie_day);
        return 1;
    }

    if (!anniversary_date(config->epoch, arvelie_year, &start)) {
        fprintf(stderr, "arvelie year is outside supported range\n");
        return 1;
    }

    if (!anniversary_date(config->epoch, arvelie_year + 1, &next)) {
        fprintf(stderr, "next epoch anniversary is outside supported range\n");
        return 1;
    }

    start_days = days_from_civil(start.year, start.month, start.day);
    next_days = days_from_civil(next.year, next.month, next.day);
    max_yday = (int)(next_days - start_days);

    if (yday < 1 || yday > max_yday) {
        fprintf(stderr, "invalid day-of-year %d for arvelie year %d\n", yday, arvelie_year);
        return 1;
    }

    result_days = start_days + (int64_t)yday - 1LL;

    if (!civil_from_days(result_days, &result)) {
        fprintf(stderr, "decoded date is outside supported range\n");
        return 1;
    }

    printf("%04d-%02d-%02d\n", result.year, result.month, result.day);
    return 0;
}

static int encode_date(const char *date_str, const ArvelieConfig *config) {
    if (config->has_epoch) {
        return encode_date_epoch(date_str, config);
    }

    return encode_date_legacy(date_str);
}

static int decode_date(const char *date_str, const ArvelieConfig *config) {
    if (config->has_epoch) {
        return decode_date_epoch(date_str, config);
    }

    return decode_date_legacy(date_str);
}

static int encode_time(const char *time_str) {
    int hour, min, sec;
    int day_sec;
    int64_t numerator;
    int beats;
    int pulses;
    int64_t beat_floor_numerator;
    int64_t pulse_remainder;

    if (!parse_time_hms(time_str, &hour, &min, &sec)) {
        fprintf(stderr, "invalid time: %s\n", time_str);
        return 1;
    }

    day_sec = hour * 3600 + min * 60 + sec;

    /*
        Lua equivalent:
          dayPerc = (daySec / 86400.0) * 1000000
          beats = floor(dayPerc / 1000)
          pulses = ceil(dayPerc % 1000)

        This keeps the same intent using integer arithmetic.
    */
    numerator = (int64_t)day_sec * 1000000LL;

    beats = (int)(numerator / (86400LL * 1000LL));

    beat_floor_numerator = (int64_t)beats * 1000LL * 86400LL;
    pulse_remainder = numerator - beat_floor_numerator;

    pulses = (int)((pulse_remainder + 86400LL - 1LL) / 86400LL);

    if (pulses == 1000) {
        beats++;
        pulses = 0;
    }

    if (beats > 999) {
        beats = 999;
        pulses = 999;
    }

    printf("%03d:%03d\n", beats, pulses);
    return 0;
}

static int decode_time(const char *time_str) {
    int beats;
    int pulses;
    int64_t total_pulses;
    int secs;
    int hour;
    int min;
    int sec;

    if (!parse_neralie(time_str, &beats, &pulses)) {
        fprintf(stderr, "invalid neralie time: %s\n", time_str);
        return 1;
    }

    /*
        Lua equivalent:
          secs = floor((pulses / 1000 + beats) * 86.4)

        Since 86.4 = 864 / 10:
          secs = floor((beats * 1000 + pulses) * 864 / 10000)
    */
    total_pulses = (int64_t)beats * 1000LL + pulses;
    secs = (int)((total_pulses * 864LL) / 10000LL);

    if (secs < 0) {
        secs = 0;
    }

    if (secs > 86399) {
        secs = 86399;
    }

    hour = secs / 3600;
    min = (secs % 3600) / 60;
    sec = secs % 60;

    printf("%02d:%02d:%02d\n", hour, min, sec);
    return 0;
}

static char *trim(char *s) {
    char *end;

    while (isspace((unsigned char)*s)) {
        s++;
    }

    if (*s == '\0') {
        return s;
    }

    end = s + strlen(s) - 1;

    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return s;
}

static int set_epoch_from_string(ArvelieConfig *config, const char *value, const char *source) {
    Date epoch;

    if (!parse_date_value(value, &epoch)) {
        fprintf(stderr, "invalid epoch from %s: %s\n", source, value);
        return 0;
    }

    config->epoch = epoch;
    config->has_epoch = 1;
    return 1;
}

static int load_config_path(ArvelieConfig *config, const char *path) {
    FILE *file;
    char line[CONFIG_LINE_MAX];

    file = fopen(path, "r");

    if (file == NULL) {
        return 1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *p = trim(line);
        char *eq;
        char *key;
        char *value;

        if (*p == '\0' || *p == '#') {
            continue;
        }

        eq = strchr(p, '=');

        if (eq == NULL) {
            continue;
        }

        *eq = '\0';
        key = trim(p);
        value = trim(eq + 1);

        if (strcmp(key, "epoch") == 0) {
            if (!set_epoch_from_string(config, value, path)) {
                fclose(file);
                return 0;
            }
        }
    }

    fclose(file);
    return 1;
}

static int load_config_file(ArvelieConfig *config) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    char path[CONFIG_PATH_MAX];
    int written;

    /*
        Load the fallback first and the XDG location second. If both files exist,
        the XDG config wins.
    */
    if (home != NULL && *home != '\0') {
        written = snprintf(path, sizeof(path), "%s/.config/timekeeper/config", home);

        if (written < 0 || (size_t)written >= sizeof(path)) {
            fprintf(stderr, "config path is too long\n");
            return 0;
        }

        if (!load_config_path(config, path)) {
            return 0;
        }
    }

    if (xdg != NULL && *xdg != '\0') {
        written = snprintf(path, sizeof(path), "%s/timekeeper/config", xdg);

        if (written < 0 || (size_t)written >= sizeof(path)) {
            fprintf(stderr, "config path is too long\n");
            return 0;
        }

        if (!load_config_path(config, path)) {
            return 0;
        }
    }

    return 1;
}

static int load_env_config(ArvelieConfig *config) {
    const char *epoch = getenv("TIMEKEEPER_EPOCH");

    if (epoch == NULL || *epoch == '\0') {
        return 1;
    }

    return set_epoch_from_string(config, epoch, "TIMEKEEPER_EPOCH");
}

static int parse_cli_options(
    int argc,
    char **argv,
    ArvelieConfig *config,
    int *cmd_index
) {
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "--epoch") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--epoch requires YYYY-MM-DD\n");
                return 0;
            }

            if (!set_epoch_from_string(config, argv[i + 1], "--epoch")) {
                return 0;
            }

            i += 2;
            continue;
        }

        if (strncmp(argv[i], "--epoch=", 8) == 0) {
            if (!set_epoch_from_string(config, argv[i] + 8, "--epoch")) {
                return 0;
            }

            i++;
            continue;
        }

        if (strcmp(argv[i], "--no-epoch") == 0) {
            config->has_epoch = 0;
            i++;
            continue;
        }

        break;
    }

    *cmd_index = i;
    return 1;
}

static void usage(const char *program) {
    fprintf(stderr,
        "usage:\n"
        "  %s [--epoch YYYY-MM-DD] encode-date YYYY-M-D\n"
        "  %s [--epoch YYYY-MM-DD] decode-date YYYY[A-Z|+]DD\n"
        "  %s encode-time HH:MM:SS\n"
        "  %s decode-time BBB:PPP\n"
        "\n"
        "epoch source priority:\n"
        "  1. --epoch YYYY-MM-DD or --epoch=YYYY-MM-DD\n"
        "  2. TIMEKEEPER_EPOCH=YYYY-MM-DD\n"
        "  3. $XDG_CONFIG_HOME/timekeeper/config\n"
        "  4. ~/.config/timekeeper/config\n"
        "\n"
        "config file format:\n"
        "  epoch=YYYY-MM-DD\n"
        "\n"
        "notes:\n"
        "  --no-epoch disables any configured epoch for one command.\n"
        "  With an epoch, arvelie year 0000 starts on the epoch date.\n",
        program, program, program, program
    );
}

int main(int argc, char **argv) {
    ArvelieConfig config;
    const char *cmd;
    const char *value;
    int cmd_index;

    config.epoch.year = 1;
    config.epoch.month = 1;
    config.epoch.day = 1;
    config.has_epoch = 0;

    if (!load_config_file(&config)) {
        return 1;
    }

    if (!load_env_config(&config)) {
        return 1;
    }

    if (!parse_cli_options(argc, argv, &config, &cmd_index)) {
        usage(argv[0]);
        return 1;
    }

    if (argc - cmd_index != 2) {
        usage(argv[0]);
        return 1;
    }

    cmd = argv[cmd_index];
    value = argv[cmd_index + 1];

    if (strcmp(cmd, "encode-date") == 0) {
        return encode_date(value, &config);
    }

    if (strcmp(cmd, "decode-date") == 0) {
        return decode_date(value, &config);
    }

    if (strcmp(cmd, "encode-time") == 0) {
        return encode_time(value);
    }

    if (strcmp(cmd, "decode-time") == 0) {
        return decode_time(value);
    }

    fprintf(stderr, "unknown command: %s\n", cmd);
    usage(argv[0]);
    return 1;
}

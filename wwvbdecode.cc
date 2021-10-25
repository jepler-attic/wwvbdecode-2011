#define __STDC_CONSTANT_MACROS 1
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifdef AVR
#define printf(...) (0)
#endif

/* Timezone setting
 *
 * If you are not in US/Central timezone, you'll have to modify this.
 *
 * standard and dst give the offsets during standard and dst times,
 * respectively.
 *
 * apply_standard and apply_dst give the UTC minute of the day to start standard
 * or dst time.
 *
 * You can use the unix 'zdump' program to see these values for your timezone:
 * $ zdump -v -c 2010,2011 US/Central
 * ...
 * US/Central  Sun Mar 14 08:00:00 2010 UTC = Sun Mar 14 03:00:00 2010 CDT isdst=1 gmtoff=-18000
 * US/Central  Sun Nov  7 07:00:00 2010 UTC = Sun Nov  7 01:00:00 2010 CST isdst=0 gmtoff=-21600
 * ..
 * From the gmtoff values in seconds, you can determine the hours and minutes of
 * offset for standard and dst.  If the minute offset is nonzero and the hour
 * offset is negative, then the minute offset should be negative too. (e.g.,
 * subtract 6 hours and then subtract 30 minutes).
 *
 * From the UTC transition times, you can directly read the values for
 * apply_standard and apply_dst.
 */
struct tzoffset { int8_t hour, minute; };
const tzoffset standard = { -6, 0 }, apply_standard = { 7, 0 };
const tzoffset dst = { -5, 0 }, apply_dst = {8, 0};

struct wwvb_t {
    int16_t yday;  // 1..365, or 1..366 in leap years
    int8_t hour;   // 0..23
    int8_t minute; // 0..60
    int8_t second; // 0..60
    int8_t year;   // 0..99, indicating 2000..2099
    unsigned ls:1; // true if a leap second is coming
    unsigned ly:1; // true if it's a leap year, at least after feb 28
    unsigned dst:2; // 2-bit dst indicator code
};

extern void set_time(const wwvb_t &t);
extern void next_second();
extern void set_divisor(uint32_t divisor);

namespace {
int8_t isly(int8_t year) {
    return (year == 0 || year % 100) && ((year % 4) == 0);
}

int16_t last_yday(wwvb_t &t) {
    return 365 + isly(t.year);
}

// When the leap second flag is set, the last minute of the last hour
// of the last day of that month has an extra second inserted
//
// This function returns 59 for most minutes, and 60 for leap minutes.
//
// Some documents indicate that leap seconds might occur a the end of any
// month, while others indicate that they occur only in June and December.
// Possibly this code should be revised to account for the possibility of
// leap seconds in other months.
uint8_t last_second(wwvb_t &t) {
    if(!t.ls) return 59;
    if(t.hour != 23) return 59;
    if(t.minute != 59) return 59;
    if(t.ly) {
        if(t.yday == 182 || t.yday == 366) return 60;
    } else {
        if(t.yday == 181 || t.yday == 365) return 60;
    }
    return 59;
}

// Advance to exactly the beginning of the next minute
void advance_minute(wwvb_t &t) {
    // If the past minute ended with a leap second, reset the flag
    if(last_second(t) == 60) t.ls = 0;

    t.second = 0;
    ++t.minute;
    if(t.minute < 60) return;

    // new hour
    t.minute = 0;
    ++t.hour;
    if(t.hour < 24) return;

    // new day
    t.hour = 0;
    ++t.yday;
    if(t.yday <= last_yday(t)) return;

    // new year
    t.yday = 1;
    t.year += 1;
    t.ly = isly(t.year);
}


// Advance by exactly one second
void advance_second(wwvb_t &t) {
    ++t.second;
    if(t.second <= last_second(t)) return;

    t.second = 0;
    advance_minute(t);
}

// Advance or decrease the time according to the timezone offset
// in minutes and seconds.  Note: for negative non-hour offsets, h and m
// should both be negative.  e.g., for instance the NST offset of -3:30
// would be passed as h=-3, m=-30
void offset_tz(wwvb_t &t, int8_t h, int8_t m) {
    t.minute += m;
    t.hour += h;
    // Now, both hours and minutes can be outside their normal range

    // Adjust minutes until they are again in the range [0..60)
    if(m < 0) {
        m += 60;
        t.hour -= 1;
    } else if(m > 60) {
        m -= 60;
        t.hour -= 1;
    }
    // Now minutes are in the range [0..60) but hours can be outside
    // the range.
    if(t.hour < 0) {
        t.hour += 24;
        t.yday -= 1;
    } else if(t.hour > 24) {
        t.hour -= 24;
        t.yday += 1;
    }
    // Now, minutes and hours are in their normal range, but the year
    // day can be outside the year.
    // in practice, only one 'if' will run
    if(t.yday <= 0) {
        t.year--;
        t.yday = last_yday(t) - t.yday;
        t.ly = isly(t.year);
    } else if(t.yday > last_yday(t)) {
        t.yday -= last_yday(t);
        t.year++;
        t.ly = isly(t.year);
    }

    // Now
}

bool isdst(wwvb_t &t)
{
    switch(t.dst)
    {
	case 0: return false;
	case 3: return true;
	case 2: // dst begins today
		if(t.hour > apply_dst.hour) return true;
		if(t.hour < apply_dst.hour) return false;
		return t.minute >= apply_dst.minute;
	case 1: // dst ends today
		if(t.hour > apply_standard.hour) return false;
		if(t.hour < apply_standard.hour) return true;
		return t.minute < apply_standard.minute;
    }
}

void apply_tz(wwvb_t &t)
{
    if(isdst(t)) offset_tz(t, dst.hour, dst.minute);
    else offset_tz(t, standard.hour, standard.minute);
}

int8_t leapyears_before(int8_t year) {
    return (year + 3) / 4;
}

// monday = 0, sunday = 6
#define DOW_YDAY1_YEAR0 5  // january 1, 2000 = saturday
int8_t get_dow(wwvb_t &t) {
    int16_t dow = DOW_YDAY1_YEAR0;
    dow += t.year;
    dow += leapyears_before(t.year);
    dow += t.yday-1;
    return dow % 7;
}

bool operator==(const wwvb_t &a, const wwvb_t &b) {
    return a.year == b.year && a.yday == b.yday
        && a.hour == b.hour && a.minute == b.minute
        && a.second == b.second
        && a.dst == b.dst && a.ly == b.ly
        && a.ls == b.ls;
}

const uint8_t NSAMPLES = 120;
const int8_t DEBOUNCE_TC = 10;
const int16_t COUNTER_SLOP = 100;
// 10 for DEBOUNCE_TC, 30 for receiver rise/fall time, 5 for light propagation
const int16_t SIGNAL_DELAY = 40;
uint8_t wwvb_buf[(NSAMPLES+3)/4];
int8_t wwvb_pos;

enum wwvb_state_t { STATE_FIND_POLARITY, STATE_CAPTURE_TIME };

wwvb_state_t wwvb_state;
bool wwvb_polarity;

int8_t wwvb_counter;
bool wwvb_denoised;

int16_t counter;
int8_t sos_counter;

void WWVB_PUT(uint8_t v) {
    uint8_t idx = wwvb_pos / 4;
    uint8_t s = 2 * (wwvb_pos % 4);
    uint8_t m = 3 << s;
    wwvb_buf[idx] = (wwvb_buf[idx] & ~m) | (v << s);
    wwvb_pos ++;
    if(wwvb_pos == NSAMPLES) wwvb_pos = 0;
}

uint8_t WWVB_GET(int8_t i) {
    i = i - NSAMPLES + wwvb_pos;
    if(i < 0) i += NSAMPLES;
    uint8_t idx = i / 4;
    uint8_t s = 2*(i % 4);
    return (wwvb_buf[idx] >> s) & 3;
}

void decode_one_minute(uint8_t pos, wwvb_t &t) {
    // The minute running from pos..pos+60 is already validated
    // so there's no need to check mark bits or "always zero" bits
    // .. everything from WWVB_GET is just a 0 or a 1
#define G(p, w) (WWVB_GET((p)+pos) ? (w) : 0)
    t.minute = G(1, 40) + G(2, 20) + G(3, 10)
            + G(5, 8) + G(6, 4) + G(7, 2) + G(8, 1);
    t.hour = G(12, 20) + G(13, 10)
            + G(15, 8) + G(16, 4) + G(17, 2) + G(18, 1);
    t.yday = G(22, 200) + G(23, 100)
            + G(25, 80) + G(26, 40) + G(27, 20) + G(28, 10)
            + G(30, 8) + G(31, 4) + G(32, 2) + G(33, 1);
    t.year = G(45, 80) + G(46, 40) + G(47, 20) + G(48, 10)
            + G(50, 8) + G(51, 4) + G(52, 2) + G(53, 1);
    t.ly = G(55, 1);
    t.ls = G(56, 1);
    t.dst = G(57, 2) + G(58, 1);
    t.second = 0;
}

void denoise_step(bool &denoised, int8_t &counter, bool value) {
    if(value) {
        if(0 && counter <= 0) counter = 1;
        else if(counter == DEBOUNCE_TC) denoised = value;
        else counter++;
    } else {
        if(0 && counter >= 0) counter = -1;
        else if(counter == -DEBOUNCE_TC) denoised = value;
        else counter--;
    }
}


bool counter_near(int16_t n) {
    return counter > (n - COUNTER_SLOP) && counter <= (n + COUNTER_SLOP);
}

const char *format_wwvbtime(const wwvb_t &t) {
    static char buf[80];
    snprintf(buf, sizeof(buf), "%4d/%03d %d:%02d:%02d ly=%d ls=%d dst=%d",
        t.year + 2000, t.yday, t.hour, t.minute, t.second, t.ls, t.ly, t.dst);
    return buf;
}

// bit i is 1 if it must be a mark, 0 if it must not be a mark
const uint64_t markmask =
    (UINT64_C(1) <<  0) |
    (UINT64_C(1) <<  9) |
    (UINT64_C(1) << 19) |
    (UINT64_C(1) << 29) |
    (UINT64_C(1) << 39) |
    (UINT64_C(1) << 49) |
    (UINT64_C(1) << 59);

#define MARK(i) do { if(WWVB_GET(i) != 2 || WWVB_GET(i+60) != 2) return false; } while(0)
#define NOMARK(i) do { if(WWVB_GET(i) == 2 || WWVB_GET(i+60) == 2) return false; } while(0)
bool try_set_time(wwvb_t &new_t) {
    // Check for markers over 2 minutes
    for(int i=0; i<60; i++)
        if(markmask & (UINT64_C(1) << i)) MARK(i); else NOMARK(i);
    wwvb_t t0, t1;
    decode_one_minute(0, t0);
    decode_one_minute(60, t1);

//printf("first minute  %s\n", format_wwvbtime(t0));
//printf("second minute %s\n", format_wwvbtime(t1));
    advance_minute(t0);
//printf("advanced      %s\n", format_wwvbtime(t0));
    if(t0 == t1) {
        t1.second = 59;
        new_t = t1;
        return true;
    }
    return false;
}

uint32_t daysec(wwvb_t &t)
{
    return t.hour * (uint32_t)3600 + t.minute * 60 + t.second;
}

// Accurate timing through PWM dithering:
// We want a nominal 1ms interrupt  for calls to wwvb_receive_loop.  Suppose
// the clocksource is nominally 16MHz.  This means we want a TOP value of 16000
// If the clocksource is 16.001MHz, then we want a TOP value of 16001, and so on
// for each .001MHz multiple of clock speed.  If the clocksource is somewhere in
// between, then the desired rate is not an integer.
//
// To give a long-term interval closer to the true interval, a fixed-point
// approximation to the divisor is made, with a fixed denominator of 32768.
// With integer math, we can compute the top value that is numerically less than
// or equal to the desired divisor:
//     top_l = divisor >> 15;
// and the fraction by which the true divisor exceeds it:
//     num = divisor & 0x7fff;
//
// Now, if num cycles have length (top_l+1) and 32768-num cycles have length
// top+l, the total time for 32768 cycles is top_l*32768 + num, which is equal
// to divisor.  Therefore, we have achieved our exact desired rate over a
// timescale of 32.768 seconds.  Because the longer and shorter cycles are
// equally distributed, the rate is actually within 1 clock of the desired rate
// at all times.  The jitter resulting from the dither is 1 clock, or 200ns at
// 5MHz.
#ifdef OXCO
// The oven-compensated crystal oscillator runs at 10MHz but is divided
// by two before being presented on the T1 pin.  Accuracy, in principle: <<1ppm
const uint32_t NOMINAL_RATE=5000*32768;
#else
// The AVR's native clock rate is 16MHz.  Accuracy, in principle: <100ppm
const uint32_t NOMINAL_RATE=16000*32768;
#endif
uint32_t divisor=NOMINAL_RATE;
int32_t ticks, last_steer_ticks;
wwvb_t last_steer_time = {999, };

/* TIMER1 runs at 16MHz nominal.  We want a 1ms interrupt.  
 * That makes the nominal TOP value 16000.  This wide adjustment range allows
 * us to accomodate real clock frequencies of 15MHz..16MHz.  The expected clock
 * variations are actually on the order of ppm, not 6%.
 */

void steer_timer(wwvb_t &pending_time)
{
    // Don't steer based on samples from within the same day
    // .. or samples more than one day apart
    if(pending_time.yday != last_steer_time.yday + 1) goto out;
    if(pending_time.year != last_steer_time.year) goto out;

    // Don't steer if a leap second might have interfered
    if(pending_time.ls != last_steer_time.ls) return;

    {
    uint32_t real_elapsed = 1000 * (daysec(pending_time) + 86400 - daysec(last_steer_time));

    // Don't steer if it's less than 22 hours
    if(real_elapsed < (22 * 60 * 60 * 1000)) return;

    uint32_t counted_elapsed = ticks - last_steer_ticks;

    printf("divisor was %f[%d]\n", divisor / 32768., divisor);
    divisor = (uint64_t)divisor * counted_elapsed / real_elapsed;
    set_divisor(divisor);
    printf("real_elapsed=%d counted_elapsed=%d steered to %f[%d]\n", real_elapsed, counted_elapsed, divisor / 32768., divisor);
    }

out:
    last_steer_time = pending_time;
    last_steer_ticks = ticks;

}


bool pending_set_time;
bool pps_good;
wwvb_t pending_time;
int16_t free_running_ms;
}

void wwvb_receive_loop(bool raw_wwvb) {
    bool old_wwvb_denoised = wwvb_denoised;
    denoise_step(wwvb_denoised, wwvb_counter, raw_wwvb);

    bool edge = wwvb_denoised ^ old_wwvb_denoised;
    bool wwvb = wwvb_denoised ^ wwvb_polarity;
    bool rising_edge = edge && wwvb;
    bool falling_edge = edge && !wwvb;

    free_running_ms ++;
    if(free_running_ms >= 1000) {
        free_running_ms -= 1000;
        next_second();
    }

printf("wwvb_receive_loop %c %4d %d %8d\n",
        "_~-+"[wwvb + 2*edge], counter, wwvb_polarity, free_running_ms);
    switch(wwvb_state) {
    case STATE_FIND_POLARITY:
        if(rising_edge) {
            if(sos_counter == 0) {
                printf(
                    "rising edge with sos_counter = 0: reset counter %d->0\n",
                    counter);
                counter = 0;
                sos_counter ++;
            } else if(counter_near(1000)) {
                sos_counter++;
                printf(
                    "good rising edge counter=%d sos_counter->%d\n",
                    counter, sos_counter);
                if(sos_counter == 10) {
                    goto set_state_capture_time;
                }
            } else {
                printf(
                    "bad rising edge counter=%d polarity->%d\n",
                    counter, !wwvb_polarity);
                sos_counter = 0; wwvb_polarity = !wwvb_polarity;
            }
        }
        break;

    case STATE_CAPTURE_TIME:
        if(rising_edge) {
            if(!counter_near(1000)) {
                printf(
                    "bad rising edge counter=%d -> find_polarity",
                    counter);
                goto set_state_find_polarity;
            }
            if(pending_set_time) {
                set_time(pending_time);
		steer_timer(pending_time);
                pending_set_time = false;
                free_running_ms = 1000 + SIGNAL_DELAY;
            }
            static uint8_t seconds_unset;
            seconds_unset ++;
            if(seconds_unset == 0) goto set_state_find_polarity;
        }
        if(falling_edge) {
            if(counter_near(200)) WWVB_PUT(0);
            else if(counter_near(500)) WWVB_PUT(1);
            else if(counter_near(800)) {
                WWVB_PUT(2);
                pending_set_time = try_set_time(pending_time);
            } else {
                printf(
                    "bad falling edge counter=%d -> find_polarity",
                    counter);
                goto set_state_find_polarity;
            }
        }

    }

    if(rising_edge) {
        pps_good = counter_near(1000);
        counter = 0;
    } else if(counter > 1000 + COUNTER_SLOP) {
        pps_good = false;
        counter ++;
    } else
        counter ++;

    return;

set_state_capture_time:
    counter = 0;
    memset(wwvb_buf, 0, sizeof(wwvb_buf));
    wwvb_state = STATE_CAPTURE_TIME;
    return;

set_state_find_polarity:
    sos_counter = 0;
    wwvb_state = STATE_FIND_POLARITY;
    return;
}

#ifndef AVR
#include <stdio.h>
#include <stdlib.h>

bool time_valid;
wwvb_t now;

void set_time(const wwvb_t &t) {
    if(time_valid) return;
    printf("set time %d/%03d %2d:%02d:%02d ly=%d ls=%d dst=%d\n",
        t.year + 2000, t.yday, t.hour, t.minute, t.second, t.ls, t.ly, t.dst);
    now = t;
    time_valid = true;
}

void set_divisor(uint32_t div) {}

void next_second() {
    if(!time_valid) return;
    advance_second(now);
    if(now.second == 0 || now.second >= 59)
    {
	wwvb_t t = now;
	printf("%d/%03d %2d:%02d:%02d.%04d ly=%d ls=%d dst=%d  ",
	    t.year + 2000, t.yday, t.hour, t.minute, t.second, free_running_ms, t.ls, t.ly, t.dst);
	apply_tz(t);
	printf("%d/%03d %2d:%02d:%02d.%04d C%cT\n",
	    t.year + 2000, t.yday, t.hour, t.minute, t.second, free_running_ms, isdst(now) ? 'D' : 'S');
    }
}

int main() {
    while(1) {
        int c = getchar();
        if(c == EOF) break;
        wwvb_receive_loop(c == '1');
    }
    if(!time_valid) printf("failed to set time\n");
}
#else
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#define LED (5)
#define WWVB (0)

#define CYCLES_HIGH 16000

int uptime;

char buf[80];
void set_time(const wwvb_t &t) {
    snprintf(buf, sizeof(buf),
        "set time %d/%03d %d:%02d:%02d ly=%d ls=%d dst=%d uptime=%d pol=%d\n",
        t.year + 2000, t.yday, t.hour, t.minute, t.second, t.ls, t.ly, t.dst,
        uptime, wwvb_polarity);
    eeprom_write_block(buf, 0, strlen(buf)+1);

    // set LED on solid
    PORTB |= (1<<LED);

    // and freeze here
    while(1) {}
}

ISR(TIMER1_CAPT_vect) {
    bool wwvb_raw = !!(PINB & (1<<WWVB));
    wwvb_receive_loop(wwvb_raw);

    int val;

    if(wwvb_state == STATE_FIND_POLARITY) {
        val = pps_good;
    } else {
        val = (wwvb_denoised ^ wwvb_polarity ) ? 1 : 16;
    }
    if((free_running_ms & 15) < val)
        PORTB |= (1<<LED);
    else
        PORTB &= ~(1<<LED);
}

void eeprom_to_serial() {
    uint8_t *i = 0;
    uint8_t c;
    while(i < (uint8_t*)256 && (c = eeprom_read_byte(i++))) {
        while(!(UCSR0A & (1<<UDRE0)))
            {}
        UDR0 = c;
    }
}

void next_second() {
    uptime++;
}

void main() {
    // Set up TIMER1 in CTC mode with ICR1 as top
    TCCR1A = (1<WGM11);
    TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS10);
    ICR1 = CYCLES_HIGH;
    TIMSK1 = (1<<ICIE1);

    // Set the UART to 19.2kb/s.
    #define F_CPU 16000000
    #define BAUD 19200
    #include <util/setbaud.h>
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;
    #if USE_2X
    UCSR0A |= (1 << U2X0);
    #else
    UCSR0A &= ~(1 << U2X0);
    #endif
    DDRD = (1<<1); // enable TXD as output

    // Set up the LED as output
    DDRB = (1<<LED);

    PORTB |= (1<<WWVB); // turn on pull-up on wwvb

    // enable interrupts
    sei();

    // perhaps our predecessor left a message
    eeprom_to_serial();

    // and sleep forever, since things happen in the interrupt only
#ifdef SLEEP_MODE_IDLE
    set_sleep_mode(SLEEP_MODE_IDLE);
    while(1) sleep_mode();
#else
    while(1) {}
#endif

}
#endif

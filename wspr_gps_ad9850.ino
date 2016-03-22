// Libraries
#include <stdint.h>
#include <SoftwareSerial.h>
#include <TinyGPS.h>

#define DDS_REF   125000000 // this is the ref osc of the AD9850 // DDS Reference Oscilliator Frequency, in Hz. (Remember, the PLL).
#define DDS_OSET  -60 // DDS Offset in Hz // adjust for your individual AD9850 board to fine tune the frequency and ensure the signal in within the wspr window

// WSPR Output Frequency
//#define WSPR_TX_A    7.040000e6  // this is the bottom of the band. The station moves about.
#define WSPR_TX_A    10.140100e6
#define WSPR_TX_B    10.140100e6  // this is the bottom of the band. The station moves about.

#define WSPR_DUTY  2 // transmit every N slices.

static byte WSPR_DATA_HOME[] = {0,1,2,3}; // add wspr values here for callsign/grid/power level // 162 bits

#define WSPR_DATA WSPR_DATA_HOME

// DDS/Arduino Connections
#define DDS_CLOCK 8  // CLK
#define DDS_LOAD  9  // FQ_UD
#define DDS_DATA  10 // DATA
#define DDS_RESET 11 // RESET

#define LED       13
#define GPS_LED   4
#define AMP_PIN   6

#define pulseHigh(pin) {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }



// GPS
TinyGPS gps;
SoftwareSerial ss(2, 3); //GPS RX 2, TX 3

// Variables
unsigned long WSPR_TXF = 0;
unsigned long startT = 0, stopT = 0;
int year;
byte month, day, hour, minute, second, hundredths, Nsatellites, ret, duty, band;
unsigned long fix_age, fail;
char sz[32];
bool newData = false;

void setup() {
    // Set all pins to output states
    pinMode(DDS_DATA, OUTPUT);
    pinMode(DDS_CLOCK, OUTPUT);
    pinMode(DDS_LOAD, OUTPUT);
    pinMode(DDS_RESET, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(GPS_LED, OUTPUT);
    pinMode(AMP_PIN, OUTPUT);

    pulseHigh(DDS_RESET);
    pulseHigh(DDS_CLOCK);
    pulseHigh(DDS_LOAD); // this pulse enables serial mode on the AD9850 - Datasheet page 12.

    // Setup RS232
    Serial.begin(115200);
    ss.begin(4800);

    Serial.print("\n\nDDS Reset   ");
    delay(900);
    frequency(0);
    delay(100);
    Serial.println("OK");

    duty = 0;

    digitalWrite(GPS_LED, LOW);
    delay(1000);
    digitalWrite(GPS_LED, HIGH);
    delay(1000);
    digitalWrite(GPS_LED, LOW);

    digitalWrite(LED, LOW);
    delay(1000);
    digitalWrite(LED, HIGH);
    delay(1000);
    digitalWrite(LED, LOW);

    digitalWrite(AMP_PIN, LOW);
    delay(1000);
    digitalWrite(AMP_PIN, HIGH);
    delay(1000);
    digitalWrite(AMP_PIN, LOW);

}

void loop() {
    ss.begin(4800);
    newData = false;
    fail++;
    //ret = feedgps();
    for (unsigned long start = millis(); millis() - start < 250;) // read GPS for 250ms was 1000
    {
        while (ss.available()) {
            char c = ss.read();
            // Serial.write(c); // uncomment this line if you want to see the GPS data flowing
            if (gps.encode(c)) // Did a new valid sentence come in?
                newData = true;
        }
    }
    if (fail == 60000) {
        digitalWrite(GPS_LED, LOW);
        Serial.println("GPS: No Data.");
    }

    if (newData) {
        Nsatellites = gps.satellites();
        Serial.print("GPS Sats: ");
        Serial.print(Nsatellites);
        Serial.println("");

        gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &fix_age);
        if (fix_age == TinyGPS::GPS_INVALID_AGE) {
            Serial.println("GPS: No Fix.");
            digitalWrite(GPS_LED, LOW);
        } else {
            digitalWrite(GPS_LED, HIGH);
            //sprintf(sz, "Date %02d/%02d/%02d, Time %02d:%02d:%02d (Sats: %02d, WSPR Duty: %d, GPS Age: %lu ms, GPS Feed: %lu).", day, month, year, hour, minute, second, Nsatellites, (duty % WSPR_DUTY), fix_age, fail);
            //Serial.println(sz);

            Serial.print("Current Time: ");
            Serial.print(minute);
            Serial.print(" S: ");
            Serial.print(second);
            Serial.println("\n");

            if (band % 2 == 0) {
                WSPR_TXF = (WSPR_TX_A + DDS_OSET) + random(10, 190); // always choose a frequency, it mixes it all up a little with the pRNG.
            } else {
                WSPR_TXF = (WSPR_TX_B + DDS_OSET) + random(10, 190); // always choose a frequency, it mixes it all up a little with the pRNG.
            }

            if ((minute % 2 == 0) && (second >= 1) && (second <= 4)) { // start transmission between 1 and 4 seconds, on every even minute
                if (duty % WSPR_DUTY == 0) {
                    Serial.print("Beginning WSPR Transmission on ");
                    Serial.print(WSPR_TXF - DDS_OSET);
                    Serial.println(" Hz.");
                    ss.end();
                    wsprTX();
                    duty++;
                    band++;
                    Serial.println(" Transmission Finished.");
                } else {
                    duty++;
                    digitalWrite(LED, LOW);
                    digitalWrite(GPS_LED, LOW);
                    delay(5000); // so we miss the window to start TX.
                    flash_led(Nsatellites, GPS_LED); // flash the number of GPSes we have.
                    flash_led(WSPR_DUTY, LED); // flash the WSPR duty.
                }


            }

            digitalWrite(LED, HIGH);
            delay(250);
            digitalWrite(LED, LOW);
            delay(250);
        }
        fail = 0;

    }
}

void frequency(unsigned long frequency) {
    unsigned long tuning_word = (frequency * pow(2, 32)) / DDS_REF;
    Serial.println(tuning_word);
    digitalWrite(DDS_LOAD, LOW);

    shiftOut(DDS_DATA, DDS_CLOCK, LSBFIRST, tuning_word);
    shiftOut(DDS_DATA, DDS_CLOCK, LSBFIRST, tuning_word >> 8);
    shiftOut(DDS_DATA, DDS_CLOCK, LSBFIRST, tuning_word >> 16);
    shiftOut(DDS_DATA, DDS_CLOCK, LSBFIRST, tuning_word >> 24);
    shiftOut(DDS_DATA, DDS_CLOCK, LSBFIRST, 0x0);

    digitalWrite(DDS_LOAD, HIGH);
}

void byte_out(unsigned char byte) {
    int i;

    for (i = 0; i < 8; i++) {
        if ((byte & 1) == 1)
            outOne();
        else
            outZero();
        byte = byte >> 1;
    }
}

void outOne() {
    digitalWrite(DDS_CLOCK, LOW);
    digitalWrite(DDS_DATA, HIGH);
    digitalWrite(DDS_CLOCK, HIGH);
    digitalWrite(DDS_DATA, LOW);
}

void outZero() {
    digitalWrite(DDS_CLOCK, LOW);
    digitalWrite(DDS_DATA, LOW);
    digitalWrite(DDS_CLOCK, HIGH);
}

void flash_led(unsigned int t, int l) {
    unsigned int i = 0;
    if (t > 25) {
        digitalWrite(l, HIGH);
        delay(2000);
        digitalWrite(l, LOW);
    } else {
        for (i = 0; i < t; i++) {
            digitalWrite(l, HIGH);
            delay(250);
            digitalWrite(l, LOW);
            delay(250);
        }
    }
}

void wsprTXtone(int t) {
    frequency((WSPR_TXF + (t * 1.4648)));
}

void wsprTX() {
    int i = 0;
    digitalWrite(AMP_PIN, HIGH);
    digitalWrite(LED, HIGH);
    for (i = 0; i < 162; i++) {
        wsprTXtone(WSPR_DATA[i]);
        delay(682);
    }
    digitalWrite(AMP_PIN, LOW);
    frequency(0);
    digitalWrite(LED, LOW);
}

static bool feedgps() {
    while (ss.available()) {
        if (gps.encode(ss.read())) {
            return true;
        }
    }
    return false;

}

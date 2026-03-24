/*
 * ============================================================
 *  Hospital Patient Registration & Follow-up Tracker
 * ============================================================
 *  Author  : Srishant Jena
 *  File    : main.c
 *  Build   : emcc main.c -o index.html
 *            gcc  main.c -o hospital   (native test)
 *
 *  Standard headers only – fully WASM / Emscripten compatible.
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Emscripten ASYNCIFY stdin integration ─────────────────── */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

/*
 * wait_for_input()
 * Asynchronous JS function that polls the global `stdinBuffer` array
 * every 50ms.  Emscripten + ASYNCIFY will suspend the C program
 * until the Promise resolves (i.e. until the user presses Send/Enter
 * in the browser UI and stdinBuffer is populated).
 */
EM_ASYNC_JS(void, wait_for_input, (), {
    return new Promise(function(resolve) {
        function check() {
            if (typeof stdinBuffer !== 'undefined' && stdinBuffer.length > 0) {
                resolve();
            } else {
                setTimeout(check, 50);
            }
        }
        check();
    });
});

/*
 * wasm_fgets()
 * Wrapper for fgets that calls wait_for_input() first when reading
 * from stdin, ensuring the browser UI has time to collect user input
 * before fgets tries to read from the (now populated) buffer.
 */
char* wasm_fgets(char* str, int n, FILE* stream) {
    if (stream == stdin) {
        clearerr(stdin);
        wait_for_input();
    }
    return fgets(str, n, stream);
}

/* Override all fgets calls to use our async-aware wrapper */
#define fgets wasm_fgets
#endif

/* ── Constants ──────────────────────────────────────────────── */
#define MAX_PATIENTS     200
#define MAX_VISITS       1000
#define PATIENTS_FILE    "patients.txt"
#define VISITS_FILE      "visits.txt"
#define FREQUENT_THRESHOLD 3        /* visits STRICTLY greater than this */

/* ── Struct Definitions ─────────────────────────────────────── */

typedef struct {
    int  id;
    char name[50];
    int  age;
    char phone[15];
} Patient;

typedef struct {
    int  patientId;
    char date[20];
    char diagnosis[100];
    char prescription[100];
} Visit;

/* ── Global In-Memory Arrays ─────────────────────────────────── */
Patient patients[MAX_PATIENTS];
Visit   visits[MAX_VISITS];
int     patientCount = 0;
int     visitCount   = 0;

/* ══════════════════════════════════════════════════════════════
 *  SECTION 1 – INPUT VALIDATION HELPERS
 * ══════════════════════════════════════════════════════════════ */

/*
 * validateAge()
 * Returns 1 if age is in [1, 120], else 0.
 */
int validateAge(int age)
{
    return (age >= 1 && age <= 120);
}

/*
 * validatePhone()
 * Returns 1 if phone is exactly 10 digit characters, else 0.
 */
int validatePhone(const char *phone)
{
    if (strlen(phone) != 10) return 0;
    for (int i = 0; i < 10; i++) {
        if (!isdigit((unsigned char)phone[i])) return 0;
    }
    return 1;
}

/*
 * isUniquePatientID()
 * Returns 1 if the given ID does not already exist, else 0.
 */
int isUniquePatientID(int id)
{
    for (int i = 0; i < patientCount; i++) {
        if (patients[i].id == id) return 0;
    }
    return 1;
}

/*
 * patientExists()
 * Returns index of patient with given ID, or -1 if not found.
 */
int patientExists(int id)
{
    for (int i = 0; i < patientCount; i++) {
        if (patients[i].id == id) return i;
    }
    return -1;
}

/*
 * flushInputBuffer()
 * Clears any leftover characters from stdin to prevent stray reads.
 */
void flushInputBuffer(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/*
 * readLine()
 * Safe single-line reader; strips trailing newline.
 * Returns 0 on success, -1 on empty or error.
 */
int readLine(char *buf, int maxLen)
{
    if (fgets(buf, maxLen, stdin) == NULL) return -1;
    buf[strcspn(buf, "\n")] = '\0';   /* strip newline */
    if (strlen(buf) == 0)  return -1;
    return 0;
}

/* ══════════════════════════════════════════════════════════════
 *  SECTION 2 – PATIENT FUNCTIONS
 * ══════════════════════════════════════════════════════════════ */

/*
 * addPatient()
 * Collects and validates patient data, then adds to in-memory array.
 * Re-prompts on invalid input.
 */
void addPatient(void)
{
    if (patientCount >= MAX_PATIENTS) {
        printf("[ERROR] Patient database is full (%d records).\n", MAX_PATIENTS);
        return;
    }

    Patient p;
    char    buffer[50];

    /* ── Patient ID ── */
    while (1) {
        printf("  Enter Patient ID (positive integer): ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) continue;
        p.id = atoi(buffer);
        if (p.id <= 0) {
            printf("  [!] ID must be a positive integer. Try again.\n");
            continue;
        }
        if (!isUniquePatientID(p.id)) {
            printf("  [!] Patient ID %d already exists. Choose a different ID.\n", p.id);
            continue;
        }
        break;
    }

    /* ── Name ── */
    while (1) {
        printf("  Enter Name (max 49 chars): ");
        if (readLine(p.name, sizeof(p.name)) == 0) break;
        printf("  [!] Name cannot be empty. Try again.\n");
    }

    /* ── Age ── */
    while (1) {
        printf("  Enter Age (1–120): ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) continue;
        p.age = atoi(buffer);
        if (!validateAge(p.age)) {
            printf("  [!] Age must be between 1 and 120. Try again.\n");
            continue;
        }
        break;
    }

    /* ── Phone ── */
    while (1) {
        printf("  Enter Phone (10 digits, no spaces): ");
        if (readLine(p.phone, sizeof(p.phone)) == -1) {
            printf("  [!] Phone cannot be empty. Try again.\n");
            continue;
        }
        if (!validatePhone(p.phone)) {
            printf("  [!] Phone must be exactly 10 numeric digits. Try again.\n");
            continue;
        }
        break;
    }

    patients[patientCount++] = p;
    printf("\n  [OK] Patient '%s' (ID: %d) registered successfully.\n\n", p.name, p.id);
}

/*
 * searchPatient()
 * Prompts for a patient ID and displays the matching record.
 */
void searchPatient(void)
{
    char buffer[20];
    int  id, idx;

    printf("  Enter Patient ID to search: ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) return;
    id  = atoi(buffer);
    idx = patientExists(id);

    if (idx == -1) {
        printf("  [!] No patient found with ID %d.\n\n", id);
        return;
    }

    printf("\n  ┌─────────────────────────────────────┐\n");
    printf("  │         PATIENT RECORD FOUND         │\n");
    printf("  ├─────────────────────────────────────┤\n");
    printf("  │ ID    : %-28d │\n", patients[idx].id);
    printf("  │ Name  : %-28s │\n", patients[idx].name);
    printf("  │ Age   : %-28d │\n", patients[idx].age);
    printf("  │ Phone : %-28s │\n", patients[idx].phone);
    printf("  └─────────────────────────────────────┘\n\n");
}

/*
 * displayPatients()
 * Lists every registered patient in a formatted table.
 */
void displayPatients(void)
{
    if (patientCount == 0) {
        printf("  [INFO] No patients registered yet.\n\n");
        return;
    }

    printf("\n  %-8s %-25s %-6s %-13s\n", "ID", "Name", "Age", "Phone");
    printf("  %s\n", "--------------------------------------------------------------");

    for (int i = 0; i < patientCount; i++) {
        printf("  %-8d %-25s %-6d %-13s\n",
               patients[i].id,
               patients[i].name,
               patients[i].age,
               patients[i].phone);
    }
    printf("\n  Total patients: %d\n\n", patientCount);
}

/* ══════════════════════════════════════════════════════════════
 *  SECTION 3 – VISIT FUNCTIONS
 * ══════════════════════════════════════════════════════════════ */

/*
 * addVisit()
 * Collects visit details for an existing patient.
 */
void addVisit(void)
{
    if (visitCount >= MAX_VISITS) {
        printf("[ERROR] Visit log is full (%d records).\n", MAX_VISITS);
        return;
    }

    char buffer[20];
    int  id, idx;
    Visit v;

    /* ── Verify patient exists ── */
    while (1) {
        printf("  Enter Patient ID for visit: ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) continue;
        id  = atoi(buffer);
        idx = patientExists(id);
        if (idx == -1) {
            printf("  [!] Patient ID %d not found. Register the patient first.\n", id);
            continue;
        }
        break;
    }
    v.patientId = id;

    /* ── Date ── */
    while (1) {
        printf("  Enter Date (YYYY-MM-DD): ");
        if (readLine(v.date, sizeof(v.date)) == 0) break;
        printf("  [!] Date cannot be empty. Try again.\n");
    }

    /* ── Diagnosis ── */
    while (1) {
        printf("  Enter Diagnosis (max 99 chars): ");
        if (readLine(v.diagnosis, sizeof(v.diagnosis)) == 0) break;
        printf("  [!] Diagnosis cannot be empty. Try again.\n");
    }

    /* ── Prescription ── */
    while (1) {
        printf("  Enter Prescription (max 99 chars): ");
        if (readLine(v.prescription, sizeof(v.prescription)) == 0) break;
        printf("  [!] Prescription cannot be empty. Try again.\n");
    }

    visits[visitCount++] = v;
    printf("\n  [OK] Visit record added for Patient ID %d on %s.\n\n", v.patientId, v.date);
}

/*
 * displayVisits()
 * Shows all visit records for a given patient ID.
 */
void displayVisits(void)
{
    char buffer[20];
    int  id, found = 0;

    printf("  Enter Patient ID to view visit history: ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) return;
    id = atoi(buffer);

    if (patientExists(id) == -1) {
        printf("  [!] Patient ID %d does not exist.\n\n", id);
        return;
    }

    printf("\n  Visit History for Patient ID: %d\n", id);
    printf("  %s\n", "--------------------------------------------------------------");

    for (int i = 0; i < visitCount; i++) {
        if (visits[i].patientId == id) {
            found++;
            printf("  Visit #%d\n", found);
            printf("    Date         : %s\n", visits[i].date);
            printf("    Diagnosis    : %s\n", visits[i].diagnosis);
            printf("    Prescription : %s\n", visits[i].prescription);
            printf("  %s\n", "- - - - - - - - - - - - - - - - - - - - - - - - - - -");
        }
    }

    if (found == 0)
        printf("  [INFO] No visits recorded for Patient ID %d.\n", id);
    else
        printf("  Total visits: %d\n", found);

    printf("\n");
}

/*
 * countVisits()
 * Returns the number of visits recorded for a given patient ID.
 */
int countVisits(int patientId)
{
    int count = 0;
    for (int i = 0; i < visitCount; i++) {
        if (visits[i].patientId == patientId) count++;
    }
    return count;
}

/*
 * showFrequentVisitors()
 * Lists all patients whose visit count exceeds FREQUENT_THRESHOLD.
 */
void showFrequentVisitors(void)
{
    int found = 0;

    printf("\n  Frequent Visitors (visits > %d)\n", FREQUENT_THRESHOLD);
    printf("  %-8s %-25s %-10s\n", "ID", "Name", "Visits");
    printf("  %s\n", "----------------------------------------------");

    for (int i = 0; i < patientCount; i++) {
        int vc = countVisits(patients[i].id);
        if (vc > FREQUENT_THRESHOLD) {
            printf("  %-8d %-25s %-10d\n", patients[i].id, patients[i].name, vc);
            found++;
        }
    }

    if (found == 0)
        printf("  [INFO] No frequent visitors found.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════════════
 *  SECTION 4 – FILE HANDLING FUNCTIONS
 * ══════════════════════════════════════════════════════════════ */

/*
 * savePatientsToFile()
 * Writes all patients[] to patients.txt in CSV format.
 * Format: id,name,age,phone
 */
void savePatientsToFile(void)
{
    FILE *fp = fopen(PATIENTS_FILE, "w");
    if (fp == NULL) {
        printf("  [ERROR] Cannot open '%s' for writing.\n\n", PATIENTS_FILE);
        return;
    }

    for (int i = 0; i < patientCount; i++) {
        fprintf(fp, "%d,%s,%d,%s\n",
                patients[i].id,
                patients[i].name,
                patients[i].age,
                patients[i].phone);
    }

    fclose(fp);
    printf("  [OK] %d patient(s) saved to '%s'.\n\n", patientCount, PATIENTS_FILE);
}

/*
 * saveVisitsToFile()
 * Writes all visits[] to visits.txt in CSV format.
 * Format: patientId,date,diagnosis,prescription
 */
void saveVisitsToFile(void)
{
    FILE *fp = fopen(VISITS_FILE, "w");
    if (fp == NULL) {
        printf("  [ERROR] Cannot open '%s' for writing.\n\n", VISITS_FILE);
        return;
    }

    for (int i = 0; i < visitCount; i++) {
        fprintf(fp, "%d,%s,%s,%s\n",
                visits[i].patientId,
                visits[i].date,
                visits[i].diagnosis,
                visits[i].prescription);
    }

    fclose(fp);
    printf("  [OK] %d visit(s) saved to '%s'.\n\n", visitCount, VISITS_FILE);
}

/*
 * saveAllToFile()
 * Convenience wrapper – saves both patients and visits.
 */
void saveAllToFile(void)
{
    savePatientsToFile();
    saveVisitsToFile();
}

/*
 * loadPatientsFromFile()
 * Reads patients.txt into the patients[] array.
 * Skips lines that are malformed.
 * Returns number of records loaded.
 */
int loadPatientsFromFile(void)
{
    FILE *fp = fopen(PATIENTS_FILE, "r");
    if (fp == NULL) {
        printf("  [INFO] '%s' not found. Starting with empty patient list.\n", PATIENTS_FILE);
        return 0;
    }

    patientCount = 0;
    char line[200];

    while (fgets(line, sizeof(line), fp) != NULL && patientCount < MAX_PATIENTS) {
        Patient p;
        /* Parse: id,name,age,phone */
        char *tok = strtok(line, ",");
        if (!tok) continue;
        p.id = atoi(tok);

        tok = strtok(NULL, ",");
        if (!tok) continue;
        strncpy(p.name, tok, sizeof(p.name) - 1);
        p.name[sizeof(p.name) - 1] = '\0';

        tok = strtok(NULL, ",");
        if (!tok) continue;
        p.age = atoi(tok);

        tok = strtok(NULL, "\n");
        if (!tok) continue;
        strncpy(p.phone, tok, sizeof(p.phone) - 1);
        p.phone[sizeof(p.phone) - 1] = '\0';

        /* Basic sanity check before accepting the record */
        if (p.id > 0 && validateAge(p.age) && validatePhone(p.phone)) {
            patients[patientCount++] = p;
        }
    }

    fclose(fp);
    printf("  [OK] Loaded %d patient(s) from '%s'.\n", patientCount, PATIENTS_FILE);
    return patientCount;
}

/*
 * loadVisitsFromFile()
 * Reads visits.txt into the visits[] array.
 * Skips records whose patientId does not exist in patients[].
 * Returns number of records loaded.
 */
int loadVisitsFromFile(void)
{
    FILE *fp = fopen(VISITS_FILE, "r");
    if (fp == NULL) {
        printf("  [INFO] '%s' not found. Starting with empty visit log.\n", VISITS_FILE);
        return 0;
    }

    visitCount = 0;
    char line[300];

    while (fgets(line, sizeof(line), fp) != NULL && visitCount < MAX_VISITS) {
        Visit v;
        char *tok = strtok(line, ",");
        if (!tok) continue;
        v.patientId = atoi(tok);

        tok = strtok(NULL, ",");
        if (!tok) continue;
        strncpy(v.date, tok, sizeof(v.date) - 1);
        v.date[sizeof(v.date) - 1] = '\0';

        tok = strtok(NULL, ",");
        if (!tok) continue;
        strncpy(v.diagnosis, tok, sizeof(v.diagnosis) - 1);
        v.diagnosis[sizeof(v.diagnosis) - 1] = '\0';

        tok = strtok(NULL, "\n");
        if (!tok) continue;
        strncpy(v.prescription, tok, sizeof(v.prescription) - 1);
        v.prescription[sizeof(v.prescription) - 1] = '\0';

        /* Accept only visits linked to a known patient */
        if (patientExists(v.patientId) != -1) {
            visits[visitCount++] = v;
        }
    }

    fclose(fp);
    printf("  [OK] Loaded %d visit(s) from '%s'.\n", visitCount, VISITS_FILE);
    return visitCount;
}

/*
 * loadAllFromFile()
 * Convenience wrapper – loads patients first (needed for visit validation),
 * then loads visits.
 */
void loadAllFromFile(void)
{
    loadPatientsFromFile();
    loadVisitsFromFile();
    printf("\n");
}

/* ══════════════════════════════════════════════════════════════
 *  SECTION 5 – MENU & MAIN
 * ══════════════════════════════════════════════════════════════ */

/*
 * printMenu()
 * Renders the top-level menu to stdout.
 */
void printMenu(void)
{
    printf("\n");
    printf("  ╔══════════════════════════════════════╗\n");
    printf("  ║   HOSPITAL PATIENT TRACKER  v1.0     ║\n");
    printf("  ╠══════════════════════════════════════╣\n");
    printf("  ║  1. Register Patient                 ║\n");
    printf("  ║  2. Add Visit Record                 ║\n");
    printf("  ║  3. Search Patient by ID             ║\n");
    printf("  ║  4. Display All Patients             ║\n");
    printf("  ║  5. Show Visit History               ║\n");
    printf("  ║  6. Frequent Visitors (visits > %d)  ║\n", FREQUENT_THRESHOLD);
    printf("  ║  7. Save Data to Files               ║\n");
    printf("  ║  8. Load Data from Files             ║\n");
    printf("  ║  9. Exit                             ║\n");
    printf("  ╚══════════════════════════════════════╝\n");
    printf("  Choice: ");
}

/*
 * main()
 * Entry point. Attempts to auto-load saved data, then runs the
 * menu loop until the user chooses Exit.
 */
int main(void)
{
    /* Disable buffering — critical for Emscripten/ASYNCIFY interactive I/O.
       Without this, printf output may not appear until a buffer fills up. */
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    setbuf(stdin,  NULL);

    char inputBuf[10];
    int  choice;

    printf("\n  Welcome to the Hospital Patient Registration & Follow-up Tracker\n");
    printf("  Attempting to load saved data...\n\n");
    loadAllFromFile();           /* silently continues if files absent */

    while (1) {
        printMenu();

        if (fgets(inputBuf, sizeof(inputBuf), stdin) == NULL) {
            /* EOF reached (e.g. piped input) – treat as exit */
            break;
        }

        choice = atoi(inputBuf);

        switch (choice) {
            case 1:
                printf("\n  ── Register New Patient ──────────────────\n");
                addPatient();
                break;

            case 2:
                printf("\n  ── Add Visit Record ──────────────────────\n");
                addVisit();
                break;

            case 3:
                printf("\n  ── Search Patient ────────────────────────\n");
                searchPatient();
                break;

            case 4:
                printf("\n  ── All Registered Patients ───────────────\n");
                displayPatients();
                break;

            case 5:
                printf("\n  ── Visit History ─────────────────────────\n");
                displayVisits();
                break;

            case 6:
                printf("\n  ── Frequent Visitors ─────────────────────\n");
                showFrequentVisitors();
                break;

            case 7:
                printf("\n  ── Saving Data ───────────────────────────\n");
                saveAllToFile();
                break;

            case 8:
                printf("\n  ── Loading Data ──────────────────────────\n");
                loadAllFromFile();
                break;

            case 9:
                printf("\n  Saving data before exit...\n");
                saveAllToFile();
                printf("  Goodbye!\n\n");
                return 0;

            default:
                printf("\n  [!] Invalid choice '%d'. Please enter 1–9.\n\n", choice);
                break;
        }
    }

    /* Auto-save if program exits via EOF */
    saveAllToFile();
    return 0;
}

/* ── End of main.c ─────────────────────────────────────────── */

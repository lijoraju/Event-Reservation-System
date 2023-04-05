#include <iostream>
#include <map>
#include "pthread.h"
#include "unistd.h"
#include "stdlib.h"
#include "queue"
#include "chrono"

using namespace std;
using namespace std::chrono;

#define MAX 5 //  maximum number of concurrent active queries
#define T 1   // total running time in mins

int mutex_AQT = 0; // used to ensures mutual exclusion while updating activeQueryTable
int turn_AQT;      // used to indicate whose turn it is while updating activeQueryTable

const int e = 1;   // num of events
const int c = 500; // seating capacity
const int s = 4;   // num of worker threads
const int t = 3;   // types of queries

struct WorkerThread
{
    pthread_t wthread;
    int threadNo;
};

typedef struct WorkerThread WorkerThread;

struct Query
{
    int type;
    int event;
    int numOfSeats;
};

typedef struct Query Query;

struct Booking
{
    int event;
    int numOfSeats;
    string bookingId;
};

typedef struct Booking Booking;

struct Request
{
    int threadNo;
    Query *query;
};

typedef struct Request Request;

struct ActiveQueryTable
{
    int event = -1;
    int type;
    int threadNo;
};

typedef struct ActiveQueryTable ActiveQueryTable;

map<int, int> events;               // key is event and value is number of available seats
map<int, vector<Booking>> bookings; // vector contains all booked tickets
vector<ActiveQueryTable> activeQueryTable;
queue<int> waitingList;
ActiveQueryTable _AQT[MAX];  // this table data structure contains currently executing queries
pthread_mutex_t lock_AQT;    // used to ensures mutual exclusion while updating _AQT
pthread_mutex_t lock_events; // used to ensures mutual exclusion while updating events table
pthread_mutex_t lock_console;

void *startReservationSystemDaemon(void *arg);
void *autoGenerateQueries(void *arg);
void enquireNumOfAvailableSeatsForEvent(int, Query);
void bookTicketsForEvent(int, Query);
void cancelBookedTicket(int, Query);
int generateRandomQueryType();
int generateRandomEventNo();
int generateRandomNumOfSeatsToBook();
int getRandomBookedTicketToCancel(int);
string generateBookingId(int);
int generateRandomInterval();

int main()
{
    pthread_t masterThread;
    if (pthread_mutex_init(&lock_AQT, NULL) != 0)
    {
        cout << "\nMutex initialization for AQT failed.";
        return 1;
    }
    if (pthread_mutex_init(&lock_events, NULL) != 0)
    {
        cout << "\nMutex initialization for events table failed.";
        return 1;
    }
    if (pthread_mutex_init(&lock_console, NULL) != 0)
    {
        cout << "\nMutex initialization for console failed.";
        return 1;
    }

    printf("\n");
    printf("<===== WELCOME TO NEHRU CENTER EVENT RESERVATION SYSTEM =====>");
    printf("\n\n");

    srand(time(0));

    pthread_create(&masterThread, NULL, startReservationSystemDaemon, NULL);
    pthread_join(masterThread, NULL);
    pthread_mutex_destroy(&lock_AQT);
    pthread_mutex_destroy(&lock_events);
    pthread_mutex_destroy(&lock_console);

    return 0;
}

/**
 * @brief Generate random query type
 * this method is called to generate a random query type inside the loop in startReservationSystemDaemon method
 * @return int
 */
int generateRandomQueryType()
{
    int randomQueryType = rand() % t + 1;
    return randomQueryType;
}

/**
 * @brief Generate random event number
 * this method will be called each time to raise a query of type 1 and type 2
 * @return int
 */
int generateRandomEventNo()
{
    int randomEvent = rand() % e + 1;
    return randomEvent;
}

/**
 * @brief Generate random num of seats to be booked for type 2 query
 * this method is called while raising a type 2 query
 * @return int
 */
int generateRandomNumOfSeatsToBook()
{
    int randomNumOfSeatsToBook = rand() % 6 + 5; // range 5 to 10
    return randomNumOfSeatsToBook;
}

/**
 * @brief Get the Random Booked Ticket To Cancel index
 * this method is called while raising a type 3 query
 * @return int
 */
int getRandomBookedTicketToCancel(int threadNo)
{
    int randomTicketIndex = rand() % bookings[threadNo].size();
    return randomTicketIndex;
}

/**
 * @brief Generates the booking Id
 * unique booking Id is generated as part of serving type 2 query
 * @param event
 * @return string
 */
string generateBookingId(int event)
{
    string bookingId = to_string(event);
    char alphabeticCode;
    static long int pseudorandomNum = 123;

    for (int i = 0; i < 3; i++)
    {
        alphabeticCode = char((rand() % 26) + 65);
        bookingId.append(1, alphabeticCode);
    }

    pseudorandomNum = (16807 * pseudorandomNum) % 2147483647;
    bookingId += to_string(pseudorandomNum);

    return bookingId;
}

int generateRandomInterval()
{
    int interval = rand() % 3 + 2;
    return interval;
}

/**
 * @brief Type 1 Query - check the available no. of tickets for an event
 * that a worker thread is assigned.
 * @param threadNo {int}
 * @param query {struct Query}
 */
void enquireNumOfAvailableSeatsForEvent(int threadNo, Query query)
{
    bool isQueryConflicting = false;
    int row;
    pthread_mutex_lock(&lock_AQT);
    for (int i = 0; i < MAX; i++)
    {
        if (_AQT[i].event == query.event && _AQT[i].type != 1)
        {
            isQueryConflicting = true;
            break;
        }
    }
    if (isQueryConflicting == false)
    {
        for (int i = 0; i < MAX; i++)
        {
            if (_AQT[i].event == -1)
            {
                _AQT[i] = {query.event, query.type, threadNo};
                row = i;
                break;
            }
        }
    }
    pthread_mutex_unlock(&lock_AQT);

    if (isQueryConflicting)
    {
        return;
    }

    cout << "\nQUERY:: Thread No " << threadNo << endl;
    cout << "::INPUT::" << endl;
    cout << " Type = " << query.type << endl;
    cout << " Event = " << query.event << endl;

    pthread_mutex_lock(&lock_events);
    sleep(generateRandomInterval());
    cout << "\nQUERY:: Thread No " << threadNo << endl;
    cout << "::OUTPUT::" << endl;
    cout << " Available Seats " << events[query.event] << endl;
    pthread_mutex_unlock(&lock_events);

    pthread_mutex_lock(&lock_AQT);
    _AQT[row] = {-1, -1, -1};
    pthread_mutex_unlock(&lock_AQT);
}

/**
 * @brief Type 2 Query - ticket booking for an event
 * a worker thread is assigned for completing ticket booking procedure - query (type 2)
 * @param threadNo {int}
 * @param query {struct Query}
 */
void bookTicketsForEvent(int threadNo, Query query)
{
    string bookingId;
    Booking ticket;
    bool isQueryConflicting = false;
    int row;

    pthread_mutex_lock(&lock_AQT);
    for (int i = 0; i < MAX; i++)
    {
        if (_AQT[i].event == query.event)
        {
            isQueryConflicting = true;
            break;
        }
    }
    if (isQueryConflicting == false)
    {
        for (int i = 0; i < MAX; i++)
        {
            if (_AQT[i].event == -1)
            {
                _AQT[i] = {query.event, query.type, threadNo};
                row = i;
                break;
            }
        }
    }
    pthread_mutex_unlock(&lock_AQT);

    if (isQueryConflicting)
    {
        return;
    }

    cout << "\nQUERY:: Thread No " << threadNo << endl;
    cout << "::INPUT::" << endl;
    cout << " Type = " << query.type << endl;
    cout << " Event = " << query.event << endl;
    cout << " No. of Tickets = " << query.numOfSeats << endl;

    pthread_mutex_lock(&lock_events);
    sleep(generateRandomInterval());
    events[query.event] -= query.numOfSeats;
    pthread_mutex_unlock(&lock_events);

    bookingId = generateBookingId(query.event);
    ticket.bookingId = bookingId;
    ticket.event = query.event;
    ticket.numOfSeats = query.numOfSeats;
    bookings[threadNo].push_back(ticket);

    cout << "\nQUERY:: Thread No " << threadNo << endl;
    cout << "::OUTPUT::" << endl;

    if (query.numOfSeats > events[query.event])
    {
        cout << " Booking Failed." << endl;
        cout << " No. of seats requested is more than the available No. of seats." << endl;
    }
    else
    {
        cout << " Booking Successfull" << endl;
        cout << " Booking Id " << ticket.bookingId << endl;
        cout << " Event " << ticket.event << endl;
        cout << " No. of Tickets Booked " << ticket.numOfSeats << endl;
    }

    pthread_mutex_lock(&lock_AQT);
    _AQT[row] = {-1, -1, -1};
    pthread_mutex_unlock(&lock_AQT);
}

void cancelBookedTicket(int threadNo, Query query)
{
    int randomTicketIndex;
    Booking ticket;
    bool isQueryConflicting = false;
    int row;

    pthread_mutex_lock(&lock_AQT);
    for (int i = 0; i < MAX; i++)
    {
        if (_AQT[i].event == query.event)
        {
            isQueryConflicting = true;
            break;
        }
    }
    if (isQueryConflicting == false)
    {
        for (int i = 0; i < MAX; i++)
        {
            if (_AQT[i].event == -1)
            {
                _AQT[i] = {query.event, query.type, threadNo};
                row = i;
                break;
            }
        }
    }
    pthread_mutex_unlock(&lock_AQT);

    if (isQueryConflicting)
    {
        return;
    }

    cout << "\nQUERY:: Thread No " << threadNo << endl;
    cout << "::INPUT::" << endl;
    cout << " Type = " << query.type << endl;

    if (bookings[threadNo].size() == 0)
    {
        cout << "::OUTPUT::" << endl;
        cout << " Sorry, you don't have any bookings to cancel." << endl;
    }
    else
    {
        randomTicketIndex = getRandomBookedTicketToCancel(threadNo);
        ticket = bookings[threadNo][randomTicketIndex];
        pthread_mutex_lock(&lock_events);
        sleep(generateRandomInterval());
        events[ticket.event] += ticket.numOfSeats;
        pthread_mutex_unlock(&lock_events);
        bookings[threadNo].erase(bookings[threadNo].begin() + randomTicketIndex);
        cout << "\nQUERY:: Thread No " << threadNo << endl;
        cout << " Choosen Random Booking Id To Cancel = " << ticket.bookingId << endl;
        cout << "::OUTPUT::" << endl;
        cout << " Booking Cancelled" << endl;
        cout << " Booking Id " << ticket.bookingId << endl;
        cout << " Event " << ticket.event << endl;
        cout << " No. of Tickets Cancelled " << ticket.numOfSeats << endl;
    }

    pthread_mutex_lock(&lock_AQT);
    _AQT[row] = {-1, -1, -1};
    pthread_mutex_unlock(&lock_AQT);
}

void *autoGenerateQueries(void *arg)
{
    WorkerThread *wthread = (WorkerThread *)arg;
    int threadNo = (*wthread).threadNo;
    auto start = high_resolution_clock::now();
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<minutes>(stop - start);

    Query query;

    while (duration < minutes(T))
    {
        query.type = generateRandomQueryType(); // random query type

        if (query.type == 1)
        {
            query.event = generateRandomEventNo(); // random event num
            enquireNumOfAvailableSeatsForEvent(threadNo, query);
        }
        else if (query.type == 2)
        {
            query.event = generateRandomEventNo();               // random event num
            query.numOfSeats = generateRandomNumOfSeatsToBook(); // random num of seats to book
            bookTicketsForEvent(threadNo, query);
        }
        else if (query.type == 3)
        {
            cancelBookedTicket(threadNo, query);
        }
        else
        {
            cout << "\nThread No " << threadNo << " Raised An Invalid Query." << endl;
        }

        sleep(5);
        stop = high_resolution_clock::now();
        duration = duration_cast<minutes>(stop - start);
    }

    pthread_exit(NULL);
    return 0;
}

/**
 * @brief Starting Reservation System Daemon
 * master thread is created for Daemon
 * @param arg
 * @return void*
 */
void *startReservationSystemDaemon(void *arg)
{
    // inside the master thread
    WorkerThread workerThreads[s];
    int error;

    // creating events list and initializing seat capacity for each event
    for (int i = 1; i <= e; i++)
    {
        events[i] = c;
    }

    // creating s worker threads
    for (int i = 0; i < s; i++)
    {
        workerThreads[i].threadNo = i + 1;
        error = pthread_create(&(workerThreads[i].wthread), NULL, autoGenerateQueries, (void *)&workerThreads[i]);

        if (error != 0)
        {
            cout << "Thread creation failed.";
        }
    }

    // waiting for all the s worker threads to complete execution
    for (int i = 0; i < s; i++)
    {
        pthread_join(workerThreads[i].wthread, NULL);
    }
    cout << "\n All worker threads completed its execution" << endl;

    // random sleep before printing current reservation status
    sleep(generateRandomInterval());

    // printing current reservation status for all events
    cout << "\nCURRENT RESERVATION STATUS" << endl;
    for (int i = 1; i <= e; i++)
    {
        cout << " \nEvent " << i << " \nBooked Seats " << c - events[i] << " \nAvailable Seats " << events[i] << endl;
    }

    // master thread is exiting
    pthread_exit(NULL);
    return 0;
}
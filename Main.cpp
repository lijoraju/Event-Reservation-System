#include <iostream>
#include <map>
#include <vector>
#include "pthread.h"
#include "unistd.h"
#include "chrono"

using namespace std;
using namespace std::chrono;

#define MAX 5      //  maximum number of concurrent active queries
#define T 2        // total running time in mins
#define TIMEOUT 10 // total no. of times that a query should go in the wait loop before timeout

const int e = 10;  // num of events
const int c = 500; // seating capacity
const int s = 20;  // num of worker threads
const int t = 3;   // types of queries

struct WorkerThread
{
    pthread_t wthread;
    int threadNo;
    bool status;
    int totalQueries;
    int executedQueries;
    int timeoutQueries;
};

typedef struct WorkerThread WorkerThread;

struct Query
{
    int type;
    int event;
    int numOfSeats;
    int queryNo;
};

typedef struct Query Query;

struct Booking
{
    int event;
    int numOfSeats;
    string bookingId;
};

typedef struct Booking Booking;

struct ActiveQueryTable
{
    int event = -1;
    int type;
    int threadNo;
};

typedef struct ActiveQueryTable ActiveQueryTable;

map<int, int> events;               // key is event and value is number of available seats
map<int, vector<Booking>> bookings; // vector contains all booked tickets
ActiveQueryTable _AQT[MAX];         // this table data structure contains currently executing queries
pthread_mutex_t lock_AQT;           // used to ensures mutual exclusion while updating _AQT
pthread_mutex_t lock_events;        // used to ensures mutual exclusion while updating events table
pthread_mutex_t lock_console;       // used to ensure mutual exclusion while printing in console

void *startReservationSystemDaemon(void *arg);
void *autoGenerateQueries(void *arg);
int enquireNumOfAvailableSeatsForEvent(int, Query);
int bookTicketsForEvent(int, Query);
int cancelBookedTicket(int, Query, int);
int beginQueryExecution(int, Query);
void completedQueryExecution(int);
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
        cout << "\nMutex initialization for events table failed.";
        return 1;
    }

    printf("\n");
    printf("<<===== WELCOME TO NEHRU CENTER EVENT RESERVATION SYSTEM =====>>");
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
    int totalBookings = bookings[threadNo].size();
    int bookingIndex;
    if (totalBookings == 0)
    {
        return -1;
    }
    bookingIndex = rand() % bookings[threadNo].size();
    return bookingIndex;
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

/**
 * @brief Generate random interval for sleep time
 * @return int
 */
int generateRandomInterval()
{
    int interval = rand() % 3 + 2; // range 2 to 4
    return interval;
}

/**
 * @brief Begining of a query execution. Adds non-conflicting query into AQT.
 * As a first the executing query must be added to AQT (Active Query Table)
 * @param threadNo {int}
 * @param query {struct Query}
 * @return int
 */
int beginQueryExecution(int threadNo, Query query)
{
    int row_AQT = -1;
    bool isQueryConflicting = false;

    pthread_mutex_lock(&lock_AQT);
    for (int i = 0; i < MAX; i++)
    {
        if (query.type == 1)
        {
            if (_AQT[i].event == query.event && _AQT[i].type != 1)
            {
                isQueryConflicting = true;
                break;
            }
        }
        else
        {
            if (_AQT[i].event == query.event)
            {
                isQueryConflicting = true;
                break;
            }
        }
    }
    if (isQueryConflicting == false)
    {
        for (int i = 0; i < MAX; i++)
        {
            if (_AQT[i].event == -1)
            {
                _AQT[i] = {query.event, query.type, threadNo};
                row_AQT = i;
                break;
            }
        }
    }
    pthread_mutex_unlock(&lock_AQT);

    return row_AQT;
}

/**
 * @brief After thread completing the query execution the query needs to be removed
 * from the AQT (Active Query Table)
 * @param row {int} - query index in the AQT Table
 */
void completedQueryExecution(int row)
{
    pthread_mutex_lock(&lock_AQT);
    _AQT[row] = {-1, -1, -1};
    pthread_mutex_unlock(&lock_AQT);
}

/**
 * @brief Type 1 Query - check the available no. of tickets for an event
 * that a worker thread is assigned.
 * @param threadNo {int}
 * @param query {struct Query}
 * @return int {status of query -1 is timeout and 1 is executed}
 */
int enquireNumOfAvailableSeatsForEvent(int threadNo, Query query)
{
    int row;
    int availableSeats;
    int timeout = TIMEOUT;

    while ((row = beginQueryExecution(threadNo, query)) == -1)
    {
        pthread_mutex_lock(&lock_console);
        cout << "\n<WAITING> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
        if (--timeout == 0)
        {
            cout << "\n<TIMEOUT> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
            cout << "::INPUT::" << endl;
            cout << " Type = " << query.type << endl;
            cout << " Event = " << query.event << endl;
            pthread_mutex_unlock(&lock_console);
            return -1;
        }
        pthread_mutex_unlock(&lock_console);
        sleep(generateRandomInterval());
    }

    pthread_mutex_lock(&lock_console);
    cout << "\n<STARTED EXECUTION> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
    cout << "::INPUT::" << endl;
    cout << " Type = " << query.type << endl;
    cout << " Event = " << query.event << endl;
    pthread_mutex_unlock(&lock_console);

    pthread_mutex_lock(&lock_events);
    sleep(generateRandomInterval());
    availableSeats = events[query.event];
    pthread_mutex_unlock(&lock_events);

    pthread_mutex_lock(&lock_console);
    cout << "\n<COMPLETED EXECUTION> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
    cout << "::OUTPUT::" << endl;
    cout << " Available Seats " << availableSeats << endl;
    pthread_mutex_unlock(&lock_console);

    completedQueryExecution(row);
    return 1;
}

/**
 * @brief Type 2 Query - ticket booking for an event
 * a worker thread is assigned for completing ticket booking procedure - query (type 2)
 * @param threadNo {int}
 * @param query {struct Query}
 * @return int {status of query -1 is timeout and 1 is executed}
 */
int bookTicketsForEvent(int threadNo, Query query)
{
    bool isBookingSuccessfull;
    string bookingId;
    Booking ticket;
    int row;
    int availableSeats;
    int timeout = TIMEOUT;

    while ((row = beginQueryExecution(threadNo, query)) == -1)
    {
        pthread_mutex_lock(&lock_console);
        cout << "\n<WAITING> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
        if (--timeout == 0)
        {
            cout << "\n<TIMEOUT> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
            cout << "::INPUT::" << endl;
            cout << " Type = " << query.type << endl;
            cout << " Event = " << query.event << endl;
            cout << " No. of Tickets = " << query.numOfSeats << endl;
            pthread_mutex_unlock(&lock_console);
            return -1;
        }
        pthread_mutex_unlock(&lock_console);
        sleep(generateRandomInterval());
    }

    pthread_mutex_lock(&lock_console);
    cout << "\n<STARTED EXECUTION> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
    cout << "::INPUT::" << endl;
    cout << " Type = " << query.type << endl;
    cout << " Event = " << query.event << endl;
    cout << " No. of Tickets = " << query.numOfSeats << endl;
    pthread_mutex_unlock(&lock_console);

    pthread_mutex_lock(&lock_events);
    sleep(generateRandomInterval());
    availableSeats = events[query.event];
    if (query.numOfSeats > availableSeats)
    {
        isBookingSuccessfull = false;
    }
    else
    {
        availableSeats = availableSeats - query.numOfSeats;
        events[query.event] = availableSeats;
        isBookingSuccessfull = true;
    }
    pthread_mutex_unlock(&lock_events);

    pthread_mutex_lock(&lock_console);
    cout << "\n<COMPLETED EXECUTION> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
    cout << "::OUTPUT::" << endl;

    if (isBookingSuccessfull == false)
    {
        cout << " Booking Failed." << endl;
        cout << " No. of seats requested is more than the available No. of seats." << endl;
    }
    else
    {
        bookingId = generateBookingId(query.event);
        ticket.bookingId = bookingId;
        ticket.event = query.event;
        ticket.numOfSeats = query.numOfSeats;
        bookings[threadNo].push_back(ticket);
        cout << " Booking Successfull" << endl;
        cout << " Booking Id " << ticket.bookingId << endl;
        cout << " Event " << ticket.event << endl;
        cout << " No. of Tickets Booked " << ticket.numOfSeats << endl;
    }
    pthread_mutex_unlock(&lock_console);

    completedQueryExecution(row);
    return 1;
}

/**
 * @brief Type 3 Query - cancel a booking for an event choosen random by the thread
 * @param threadNo {int}
 * @param query {struct Query}
 * @param randomTicketIndex {int} - thread choose one random index from its private booking list
 * @return int {status of query -1 is timeout and 1 is executed}
 */
int cancelBookedTicket(int threadNo, Query query, int randomTicketIndex)
{
    bool isCancellationSuccessfull;
    int availableSeats;
    int cancelledSeats;
    int row;
    Booking ticket;
    int timeout = TIMEOUT;

    if (randomTicketIndex != -1)
    {
        ticket = bookings[threadNo][randomTicketIndex];
        while ((row = beginQueryExecution(threadNo, query)) == -1)
        {
            pthread_mutex_lock(&lock_console);
            cout << "\n<WAITING> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
            if (--timeout == 0)
            {
                cout << "\n<TIMEOUT> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
                cout << "::INPUT::" << endl;
                cout << " Type = " << query.type << endl;
                cout << " Choosen Random Booking Index = " << randomTicketIndex << endl;
                pthread_mutex_unlock(&lock_console);
                return -1;
            }
            pthread_mutex_unlock(&lock_console);
            sleep(generateRandomInterval());
        }
    }

    pthread_mutex_lock(&lock_console);
    cout << "\n<STARTED EXECUTION> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
    cout << "::INPUT::" << endl;
    cout << " Type = " << query.type << endl;
    pthread_mutex_unlock(&lock_console);

    pthread_mutex_lock(&lock_events);
    sleep(generateRandomInterval());
    if (randomTicketIndex == -1)
    {
        isCancellationSuccessfull = false;
    }
    else
    {
        bookings[threadNo].erase(bookings[threadNo].begin() + randomTicketIndex);
        availableSeats = events[ticket.event];
        cancelledSeats = ticket.numOfSeats;
        availableSeats = availableSeats + cancelledSeats;
        events[ticket.event] = availableSeats;
        isCancellationSuccessfull = true;
    }
    pthread_mutex_unlock(&lock_events);

    pthread_mutex_lock(&lock_console);
    cout << "\n<COMPLETED EXECUTION> QUERY::" << query.queryNo << " Thread No " << threadNo << endl;
    cout << "::OUTPUT::" << endl;

    if (isCancellationSuccessfull == false)
    {
        cout << " Booking Cancellation Failed." << endl;
        cout << " Sorry, you don't have any bookings to cancel." << endl;
        pthread_mutex_unlock(&lock_console);
    }
    else
    {
        cout << " Booking Cancelled" << endl;
        cout << " Booking Id " << ticket.bookingId << endl;
        cout << " Event " << ticket.event << endl;
        cout << " No. of Tickets " << ticket.numOfSeats << endl;
        pthread_mutex_unlock(&lock_console);
        completedQueryExecution(row);
    }

    return 1;
}

/**
 * @brief Periodically auto query generation by the worker threads created by the master thread
 * @param arg {struct WorkerThread}
 * @return void*
 */
void *autoGenerateQueries(void *arg)
{
    WorkerThread *wthread = (WorkerThread *)arg;
    int threadNo = (*wthread).threadNo;
    auto start = high_resolution_clock::now();
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<minutes>(stop - start);
    int totalQueries = 0;
    int executedQueries = 0;
    int timeoutQueries = 0;
    int randomTicket;

    Query query;

    while (duration < minutes(T))
    {
        query.type = generateRandomQueryType(); // random query type
        query.queryNo = totalQueries + 1;
        totalQueries += 1;

        if (query.type == 1)
        {
            query.event = generateRandomEventNo(); // random event num

            if (enquireNumOfAvailableSeatsForEvent(threadNo, query) == -1)
            {
                timeoutQueries += 1;
            }
            else
            {
                executedQueries += 1;
            }
        }
        else if (query.type == 2)
        {
            query.event = generateRandomEventNo();               // random event num
            query.numOfSeats = generateRandomNumOfSeatsToBook(); // random num of seats to book

            if (bookTicketsForEvent(threadNo, query) == -1)
            {
                timeoutQueries += 1;
            }
            else
            {
                executedQueries += 1;
            }
        }
        else if (query.type == 3)
        {
            randomTicket = getRandomBookedTicketToCancel(threadNo);

            if (cancelBookedTicket(threadNo, query, randomTicket) == -1)
            {
                timeoutQueries += 1;
            }
            else
            {
                executedQueries += 1;
            }
        }
        else
        {
            cout << "\nThread No " << threadNo << " Raised An Invalid Query." << endl;
        }

        sleep(generateRandomInterval());
        stop = high_resolution_clock::now();
        duration = duration_cast<minutes>(stop - start);
    }

    (*wthread).totalQueries = totalQueries;
    (*wthread).executedQueries = executedQueries;
    (*wthread).timeoutQueries = timeoutQueries;

    pthread_exit(NULL);
    return 0;
}

/**
 * @brief Starting reservation system daemon
 * master thread is created for Daemon
 * @param arg {NULL}
 * @return void*
 */
void *startReservationSystemDaemon(void *arg)
{
    // inside the master thread
    WorkerThread workerThreads[s];
    int error;
    int overallTotal = 0;
    int overallExecuted = 0;
    int overallTimeout = 0;

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
            workerThreads[i].status = false;
        }
        else
        {
            workerThreads[i].status = true;
        }
    }

    // waiting for all the s worker threads to complete execution
    for (int i = 0; i < s; i++)
    {
        pthread_join(workerThreads[i].wthread, NULL);
    }
    cout << "\n *** All worker threads completed its execution ***" << endl;
    for (int i = 0; i < s; i++)
    {
        if (workerThreads[i].status)
        {
            overallTotal += workerThreads[i].totalQueries;
            overallExecuted += workerThreads[i].executedQueries;
            overallTimeout += workerThreads[i].timeoutQueries;
            cout << "\n Thread No: " << workerThreads[i].threadNo << " Total Queries: " << workerThreads[i].totalQueries << " Executed Queries: " << workerThreads[i].executedQueries << " Timeout Queries: " << workerThreads[i].timeoutQueries << endl;
        }
    }
    cout << "\n::Overall::\n Total Queries = " << overallTotal << "\n Executed Queries = " << overallExecuted << "\n Timeout Queries = " << overallTimeout << endl;

    // random sleep before printing current reservation status
    sleep(generateRandomInterval());

    // printing current reservation status for all events
    cout << "\n<<<<< CURRENT RESERVATION STATUS >>>>>" << endl;
    for (int i = 1; i <= e; i++)
    {
        cout << "\nEvent " << i << "\nBooked Seats " << c - events[i] << "\nAvailable Seats " << events[i] << endl;
    }

    // master thread is exiting
    pthread_exit(NULL);
    return 0;
}
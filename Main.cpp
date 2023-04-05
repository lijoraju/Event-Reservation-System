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
    int event;
    int type;
    int threadNo;
};

typedef struct ActiveQueryTable ActiveQueryTable;

// WorkerThread workerThreads[s];      // worker threads array
map<int, int> events;               // key is event and value is number of available seats
map<int, vector<Booking>> bookings; // vector contains all booked tickets
vector<ActiveQueryTable> activeQueryTable;
queue<int> waitingList;

void *startReservationSystemServer(void *arg);
void *autoGenerateQueries(void *arg);
void *enquireNumOfAvailableSeatsForEvent(void *arg);
void *bookTicketsForEvent(void *arg);
void *cancelBookedTicket(void *arg);
void executeQueryRequest(Request *request);
void createNewQueryEntry(Request *, ActiveQueryTable *);
void addToActiveQueryTable(ActiveQueryTable, int &);
void removeFromActiveQueryTable(int, int);
int generateRandomQueryType();
int generateRandomEventNo();
int generateRandomNumOfSeatsToBook();
int getRandomBookedTicketToCancel(int);
string generateBookingId(int);
int generateRandomInterval();

int main()
{
    pthread_t masterThread;

    printf("\n");
    printf("<===== WELCOME TO NEHRU CENTER EVENT RESERVATION SYSTEM =====>");
    printf("\n\n");

    srand(time(0));

    pthread_create(&masterThread, NULL, startReservationSystemServer, NULL);
    pthread_join(masterThread, NULL);

    return 0;
}

/**
 * @brief Generate random query type
 * this method is called to generate a random query type inside the loop in startReservationSystemServer method
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

void executeQueryRequest(Request *request)
{
    if (request == NULL)
    {
        cout << "\nRequest is NULL" << endl;
        return;
    }

    Query *query;
    query = request->query;
    if (query == NULL)
    {
        cout << "\nQuery is NULL." << endl;
        return;
    }

    if (query->type == 1)
    {
        // pthread_create(&workerThreads[request->threadNo], NULL, enquireNumOfAvailableSeatsForEvent, (void *)request);
    }
    else if (query->type == 2)
    {
        // pthread_create(&workerThreads[request->threadNo], NULL, bookTicketsForEvent, (void *)request);
    }
    else if (query->type == 3)
    {
        // pthread_create(&workerThreads[request->threadNo], NULL, cancelBookedTicket, (void *)request);
    }
    else
    {
        cout << "\nInvalid query!" << endl;
    }
}

void createNewQueryEntry(Request *request, ActiveQueryTable *row)
{
    Query *query = request->query;
    if (request == NULL)
    {
        cout << "\nError occurred while creating query entry." << endl;
        return;
    }
    if (query == NULL)
    {
        cout << "\nError occurred while creating query entry." << endl;
        return;
    }

    row->event = query->event;
    row->threadNo = request->threadNo;
    row->type = query->type;
}

void addToActiveQueryTable(ActiveQueryTable row, int &status)
{
    int tableSize;
    bool conflict;
    int nextTurn;

    if (mutex_AQT == 0)
    {
        turn_AQT = row.threadNo; // set turn to this thread
        mutex_AQT = 1;           // lock
    }

    if (turn_AQT != row.threadNo)
    {
        // push to queue
        waitingList.push(row.threadNo);
    }

wait:
    while (turn_AQT != row.threadNo && mutex_AQT == 1)
    {
        // waiting for turn
    }

    conflict = false;
    tableSize = activeQueryTable.size();

    if (tableSize >= MAX)
    {
        waitingList.push(row.threadNo);
        nextTurn = waitingList.front();
        waitingList.pop();
        turn_AQT = nextTurn;
        goto wait;
    }

    for (int i = 0; i < tableSize; i++)
    {
        if (row.type == 1)
        {
            if (activeQueryTable[i].event == row.event && activeQueryTable[i].type != 1)
            {
                conflict = true;
                break;
            }
        }
        else
        {
            if (activeQueryTable[i].event == row.event)
            {
                conflict = true;
                break;
            }
        }
    }

    if (conflict)
    {
        waitingList.push(row.threadNo);
    }
    else
    {
        activeQueryTable.push_back(row);
        status = 1;
    }

    if (!waitingList.empty())
    {
        nextTurn = waitingList.front();
        waitingList.pop();
        turn_AQT = nextTurn;
        if (conflict)
        {
            goto wait;
        }
    }
    else
    {
        mutex_AQT = 0; // unlock
    }
}

void removeFromActiveQueryTable(int threadNo, int event)
{
    vector<ActiveQueryTable>::iterator delIterator;
    int size;
    int nextTurn;

    if (mutex_AQT == 0)
    {
        turn_AQT = threadNo; // set turn to this thread
        mutex_AQT = 1;       // lock
    }

    if (turn_AQT != threadNo)
    {
        // push to queue
        waitingList.push(threadNo);
    }
    while (turn_AQT != threadNo && mutex_AQT == 1)
    {
        // waiting for turn
    }

    size = activeQueryTable.size();
    for (int i = 0; i < size; i++)
    {
        if (activeQueryTable[i].threadNo == threadNo && activeQueryTable[i].event == event)
        {
            delIterator = activeQueryTable.begin() + i;
            break;
        }
    }
    activeQueryTable.erase(delIterator);

    if (!waitingList.empty())
    {
        nextTurn = waitingList.front();
        waitingList.pop();
        turn_AQT = nextTurn;
    }
    else
    {
        mutex_AQT = 0; // unlock
    }
}

/**
 * @brief Check the available no. of tickets for an event
 * a worker thread is assigned for servicing the request - query (type 1)
 * @param arg
 * @return void*
 */
void *enquireNumOfAvailableSeatsForEvent(void *arg)
{
    int status;
    ActiveQueryTable *row;
    Request *request = (Request *)arg;
    if (request == NULL)
    {
        cout << "\nError occurred. Request is NULL" << endl;
        return 0;
    }

    Query *query = request->query;
    if (query == NULL)
    {
        cout << "\nError occurred. Query is NULL" << endl;
        return 0;
    }

    row = (ActiveQueryTable *)malloc(sizeof(ActiveQueryTable));
    if (row == NULL)
    {
        cout << "\nFailed to create new entry for query!!" << endl;
        return 0;
    }

    createNewQueryEntry(request, row);
    addToActiveQueryTable(*row, status);
    sleep(generateRandomInterval());

    // query failed to ensure database consistency
    if (status != 1)
    {
        printf("\nThread %d Failed", request->threadNo);
        // return 0;
    }

    cout << "\nQUERY:: Thread No " << request->threadNo << endl;
    cout << " Type = " << query->type << endl;
    cout << " Event = " << query->event << endl;
    cout << "::OUTPUT::" << endl;
    cout << " Available Seats " << events[query->event] << endl;

    removeFromActiveQueryTable(request->threadNo, query->event);
    // pthread_exit(NULL);

    return 0;
}

/**
 * @brief Ticket booking for an event
 * a worker thread is assigned for completing ticket booking procedure - query (type 2)
 * @param arg
 * @return void*
 */
void *bookTicketsForEvent(void *arg)
{
    string bookingId;
    int status;
    Booking ticket;
    ActiveQueryTable *row;
    Request *request = (Request *)arg;
    if (request == NULL)
    {
        cout << "\nError occurred." << endl;
        return 0;
    }

    Query *query = request->query;
    if (query == NULL)
    {
        cout << "\nEvent Invalid." << endl;
        return 0;
    }
    if (query->numOfSeats > events[query->event])
    {
        cout << "\nBooking Failed." << endl;
        cout << "\nNo. of seats requested is more than the available No. of seats." << endl;
        return 0;
    }

    row = (ActiveQueryTable *)malloc(sizeof(ActiveQueryTable));
    if (row == NULL)
    {
        cout << "\nFailed to create new entry for query!!" << endl;
        return 0;
    }

    createNewQueryEntry(request, row);
    addToActiveQueryTable(*row, status);
    sleep(generateRandomInterval());

    // query failed to ensure database consistency
    if (status != 1)
    {
        printf("\nThread %d Failed", request->threadNo);
        // return 0;
    }

    events[query->event] -= query->numOfSeats;
    bookingId = generateBookingId(query->event);
    ticket.bookingId = bookingId;
    ticket.event = query->event;
    ticket.numOfSeats = query->numOfSeats;
    bookings[request->threadNo].push_back(ticket);

    cout << "\nQUERY:: Thread No " << request->threadNo << endl;
    cout << " Type = " << query->type << endl;
    cout << " Event = " << query->event << endl;
    cout << " No. of Tickets = " << query->numOfSeats << endl;
    cout << "::OUTPUT::" << endl;
    cout << " Booking Successfull" << endl;
    cout << " Booking Id " << ticket.bookingId << endl;
    cout << " Event " << ticket.event << endl;
    cout << " No. of Tickets Booked " << ticket.numOfSeats << endl;

    removeFromActiveQueryTable(request->threadNo, query->event);
    // pthread_exit(NULL);

    return 0;
}

void *cancelBookedTicket(void *arg)
{
    int status;
    int randomTicketIndex;
    Booking ticket;
    ActiveQueryTable *row;
    Request *request = (Request *)arg;
    if (request == NULL)
    {
        cout << "\nError occurred." << endl;
        return 0;
    }

    if (bookings[request->threadNo].size() == 0)
    {
        sleep(generateRandomInterval());
        cout << "\nQUERY:: Thread No " << request->threadNo << endl;
        cout << " Type = " << request->query->type << endl;
        cout << "::OUTPUT::" << endl;
        cout << " Sorry, you don't have any bookings to cancel." << endl;
        return 0;
    }

    randomTicketIndex = getRandomBookedTicketToCancel(request->threadNo);
    ticket = bookings[request->threadNo][randomTicketIndex];
    request->query->event = ticket.event;

    row = (ActiveQueryTable *)malloc(sizeof(ActiveQueryTable));
    if (row == NULL)
    {
        cout << "\nFailed to create new entry for query!!" << endl;
        return 0;
    }

    createNewQueryEntry(request, row);
    addToActiveQueryTable(*row, status);
    sleep(generateRandomInterval());

    // query failed to ensure database consistency
    if (status != 1)
    {
        printf("\nThread %d Failed", request->threadNo);
        // return 0;
    }

    bookings[request->threadNo].erase(bookings[request->threadNo].begin() + randomTicketIndex);
    events[ticket.event] += ticket.numOfSeats;

    cout << "\nQUERY:: Thread No " << request->threadNo << endl;
    cout << " Type = " << request->query->type << endl;
    cout << " Choosen Random Booking Id = " << ticket.bookingId << endl;
    cout << "::OUTPUT::" << endl;
    cout << " Booking Cancelled" << endl;
    cout << " Booking Id " << ticket.bookingId << endl;
    cout << " Event " << ticket.event << endl;
    cout << " No. of Tickets Cancelled " << ticket.numOfSeats << endl;

    removeFromActiveQueryTable(request->threadNo, ticket.event);
    // pthread_exit(NULL);

    return 0;
}

void *autoGenerateQueries(void *arg)
{
    WorkerThread *wthread = (WorkerThread *)arg;
    int threadNo = (*wthread).threadNo;
    auto start = high_resolution_clock::now();
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<minutes>(stop - start);

    while (duration < minutes(T))
    {
        printf("\nMy Thread No is %d", threadNo);
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
void *startReservationSystemServer(void *arg)
{
    WorkerThread workerThreads[s];
    Query *query;
    Request *request;
    auto start = high_resolution_clock::now();
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<minutes>(stop - start);

    // creating events list and initializing available seats in each event
    for (int i = 1; i <= e; i++)
    {
        events[i] = c;
    }

    // creating s threads
    for (int i = 0; i < s; i++)
    {
        workerThreads[i].threadNo = i + 1;
        pthread_create(&(workerThreads[i].wthread), NULL, autoGenerateQueries, (void *)&workerThreads[i]);
    }

    // s threads that run concurrently in a loop and generate random queries
    /*
    while (duration < minutes(T))
    {
        for (int i = 1; i < s; i++)
        {
            query = (Query *)malloc(sizeof(query));
            if (query == NULL)
            {
                cout << "Out of Memory!" << endl;
                return 0;
            }

            query->type = generateRandomQueryType(); // random query type

            if (query->type == 1)
            {
                query->event = generateRandomEventNo(); // random event num for type 1 query
            }
            else if (query->type == 2)
            {
                query->event = generateRandomEventNo();               // random event num for type 2 query
                query->numOfSeats = generateRandomNumOfSeatsToBook(); // random num of seats to book for type 2 query
            }
            else if (query->type == 3)
            {
                // remove latter
            }
            else
            {
                cout << "\nQUERY ";
                cout << "Type " << query->type << endl;
                cout << "Unable to process. Invalid Query Type!" << endl;
                // free(query);
                sleep(generateRandomInterval());
                continue;
            }

            request = (Request *)malloc(sizeof(request));
            if (request == NULL)
            {
                cout << "Out of Memory!" << endl;
                return 0;
            }
            request->threadNo = i;
            request->query = query;

            executeQueryRequest(request);
            sleep(3);
        }

        // waiting for all worker threads to complete execution
        for (int i = 1; i < s; i++)
        {
            pthread_join(workerThreads[i], NULL);
        }

        stop = high_resolution_clock::now();
        duration = duration_cast<minutes>(stop - start);
    }*/

    // waiting for all worker threads to complete execution
    for (int i = 0; i < s; i++)
    {
        pthread_join(workerThreads[i].wthread, NULL);
    }

    sleep(generateRandomInterval());
    cout << "\nCURRENT RESERVATION STATUS" << endl;
    for (int i = 1; i <= e; i++)
    {
        cout << " \nEvent " << i << " \nBooked Seats " << c - events[i] << " \nAvailable Seats " << events[i] << endl;
    }
    pthread_exit(NULL);
    return 0;
}
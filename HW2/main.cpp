#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <vector>
#include <algorithm>
#include <queue>
#include <time.h>
#include <pthread.h>
#include "monitor.h"
#include "WriteOutput.h"
#include "helper.h"


// Structure to represent a car
struct PathObj{
    char connectorType;
    int connectorID;
    int from;
    int to;
};
struct Car {
    int ID;
    int travelTime;
    int pathLength;
    std::vector<PathObj> path; // Pair of (connector type, connector ID)
};


void print_queue(std::queue<int> q)
{
    while (!q.empty())
    {
        std::cout << q.front() << " ";
        q.pop();
    }
    std::cout << std::endl;
}

void printvec(std::vector<int> vec){
    for(auto i: vec){
        std::cout << i << " ";
    }
    std::cout << std::endl;
}

class NarrowBridge: public Monitor {
    pthread_cond_t emptyRoad[2];
    pthread_cond_t noCarPassing;
    pthread_cond_t first_guy;
public:
    int ID;
    int travelTime;
    int maximumWaitTime;
    int numOfCarPassing;
    int curDirection;
    std::queue<int> cars[2]; // contains car ID's 

    pthread_mutex_t  mut;

    NarrowBridge(){
        curDirection = 1; // 1 means from 0 to 1, 0 means from 1 to 0
        numOfCarPassing = 0;

        pthread_cond_init(&emptyRoad[0], NULL);
        pthread_cond_init(&emptyRoad[1], NULL);
        pthread_cond_init(&noCarPassing, NULL);
        pthread_cond_init(&first_guy, NULL);
        pthread_mutex_init(&mut, NULL);
    }
    void pass(Car car, int pathIdx) {
        pthread_mutex_lock(&mut);
        PathObj connector = car.path[pathIdx];
        WriteOutput(car.ID, connector.connectorType, connector.connectorID, ARRIVE);
        cars[connector.from].push(car.ID);
        while(true){
            if(curDirection == connector.to){
                if(cars[connector.from].front() == car.ID){
                    if(numOfCarPassing > 0){
                        pthread_mutex_unlock(&mut);
                        sleep_milli(PASS_DELAY);
                        pthread_mutex_lock(&mut);
                        if(curDirection != connector.to){
                            continue;
                        } 
                    }
                    //std::cout <<"Lane 0 has "; print_queue(cars[0]); std::cout << "Lane 1 has "; print_queue(cars[1]);
                    cars[connector.from].pop();
                    pthread_cond_broadcast(&emptyRoad[connector.from]);
                    
                    WriteOutput(car.ID, connector.connectorType, connector.connectorID, START_PASSING);
                    numOfCarPassing++;
                    pthread_mutex_unlock(&mut);
                    sleep_milli(travelTime);
                    pthread_mutex_lock(&mut);

                    WriteOutput(car.ID, connector.connectorType, connector.connectorID, FINISH_PASSING);
                    
                    numOfCarPassing--;
                    if(numOfCarPassing == 0) {
                        pthread_cond_broadcast(&noCarPassing);
                    }
                    if(numOfCarPassing == 0 && cars[connector.from].empty()){
                        pthread_cond_broadcast(&first_guy);
                    }
                    break;
                }
                else{
                    pthread_cond_wait(&emptyRoad[connector.from], &mut);
                }
            }
            else{
                if(cars[connector.to].empty() && numOfCarPassing==0){
                    curDirection = connector.to;
                }
                else if(cars[connector.from].front() != car.ID){
                    pthread_cond_wait(&emptyRoad[connector.from], &mut);
                }
                else{
                    auto now = std::chrono::system_clock::now();
                    auto maxWaitNano = std::chrono::nanoseconds(maximumWaitTime * 1000000);
                    auto waitUntil = now + maxWaitNano;
                    timespec wait_time;
                    wait_time.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(waitUntil.time_since_epoch()).count();
                    wait_time.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(waitUntil.time_since_epoch()).count() % 1000000000;
                    pthread_cond_timedwait(&first_guy, &mut, &wait_time);
                    
                    curDirection = connector.to;
                    while(numOfCarPassing > 0){
                        pthread_cond_wait(&noCarPassing, &mut);
                    }
                }
            }
        }
        pthread_mutex_unlock(&mut);
    }
};

class Ferry: public Monitor {   // inherit from Monitor
    pthread_cond_t isFull[2];     // condition varibles
public:
    int travelTime;
    int maximumWaitTime;
    int capacity;
    std::vector<int> ferry_arr[2];

    pthread_mutex_t  mut;

    Ferry(){
        pthread_cond_init(&isFull[0], NULL);
        pthread_cond_init(&isFull[1], NULL);
        
        pthread_mutex_init(&mut, NULL);
    }
    void pass(Car car, int pathIdx) {
        pthread_mutex_lock(&mut);
        
        PathObj connector = car.path[pathIdx];
        WriteOutput(car.ID, connector.connectorType, connector.connectorID, ARRIVE);
        ferry_arr[connector.from].push_back(car.ID);
        //std::cout << "0 has: "; printvec(ferry_arr[0]); std::cout << "1 has: "; printvec(ferry_arr[1]);

        if(capacity != 1 && ferry_arr[connector.from].size() == 1){ // first guy waits
            auto now = std::chrono::system_clock::now();
            auto maxWaitNano = std::chrono::nanoseconds(maximumWaitTime * 1000000);
            auto waitUntil = now + maxWaitNano;
            timespec wait_time;
            wait_time.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(waitUntil.time_since_epoch()).count();
            wait_time.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(waitUntil.time_since_epoch()).count() % 1000000000;
            pthread_cond_timedwait(&isFull[connector.from], &mut, &wait_time);
            if(std::find(ferry_arr[connector.from].begin(), ferry_arr[connector.from].end(), car.ID) != ferry_arr[connector.from].end()){
                pthread_cond_broadcast(&isFull[connector.from]);
                ferry_arr[connector.from].clear();
            }
            WriteOutput(car.ID, connector.connectorType, connector.connectorID, START_PASSING);
            pthread_mutex_unlock(&mut);
            sleep_milli(travelTime);
            pthread_mutex_lock(&mut);
            
            WriteOutput(car.ID, connector.connectorType, connector.connectorID, FINISH_PASSING);
        }            
        else if(ferry_arr[connector.from].size() == capacity){
            ferry_arr[connector.from].clear();
            pthread_cond_broadcast(&isFull[connector.from]);
            WriteOutput(car.ID, connector.connectorType, connector.connectorID, START_PASSING);
            pthread_mutex_unlock(&mut);
            sleep_milli(travelTime);
            pthread_mutex_lock(&mut);
            WriteOutput(car.ID, connector.connectorType, connector.connectorID, FINISH_PASSING);
            
        }
        else{
            pthread_cond_wait(&isFull[connector.from], &mut);
            WriteOutput(car.ID, connector.connectorType, connector.connectorID, START_PASSING);
            pthread_mutex_unlock(&mut);
            sleep_milli(travelTime);
            pthread_mutex_lock(&mut);
            WriteOutput(car.ID, connector.connectorType, connector.connectorID, FINISH_PASSING);
        }
        pthread_mutex_unlock(&mut);
    }
};

class Crossroad: public Monitor {   // inherit from Monitor
    pthread_cond_t road_condition[4];     // condition varibles
    pthread_cond_t timed_waiter[4];
    pthread_cond_t noCarPassing;
public:
    int ID;
    int travelTime;
    int maximumWaitTime;
    int numOfCarPassing;
    int curDirection;
    bool ramazan_cond;

    std::queue<int> cars[4];

    pthread_mutex_t  mut;

    Crossroad(){
        curDirection = 0;
        numOfCarPassing = 0;
        ramazan_cond = true;
        pthread_cond_init(&timed_waiter[0], NULL);
        pthread_cond_init(&timed_waiter[1], NULL);
        pthread_cond_init(&timed_waiter[2], NULL);
        pthread_cond_init(&timed_waiter[3], NULL);
        
        pthread_cond_init(&noCarPassing, NULL);
        
        pthread_cond_init(&road_condition[0], NULL);
        pthread_cond_init(&road_condition[1], NULL);
        pthread_cond_init(&road_condition[2], NULL);
        pthread_cond_init(&road_condition[3], NULL);
        
        pthread_mutex_init(&mut, NULL);
    }
    void pass(Car car, int pathIdx) {
        pthread_mutex_lock(&mut);
        
        PathObj connector = car.path[pathIdx];
        WriteOutput(car.ID, connector.connectorType, connector.connectorID, ARRIVE);
        cars[connector.from].push(car.ID);
        while(true){
            if(curDirection == connector.from){
                if(cars[connector.from].front() == car.ID){
                    if(numOfCarPassing > 0){
                        pthread_mutex_unlock(&mut);
                        sleep_milli(PASS_DELAY);
                        pthread_mutex_lock(&mut);
                        if(curDirection != connector.from){
                            continue;
                        } 
                    }
                    //std::cout <<"Lane 0 has "; print_queue(cars[0]); std::cout << "Lane 1 has "; print_queue(cars[1]); std::cout << "Lane 2 has "; print_queue(cars[2]); std::cout << "Lane 3 has "; print_queue(cars[3]);
                    cars[connector.from].pop();
                    pthread_cond_broadcast(&road_condition[connector.from]);
                    
                    WriteOutput(car.ID, connector.connectorType, connector.connectorID, START_PASSING);
                    numOfCarPassing++;
                    
                    pthread_mutex_unlock(&mut);
                    sleep_milli(travelTime);
                    pthread_mutex_lock(&mut);

                    WriteOutput(car.ID, connector.connectorType, connector.connectorID, FINISH_PASSING);
                    numOfCarPassing--;

                    if(numOfCarPassing == 0) {
                        pthread_cond_broadcast(&noCarPassing);
                    }
                    if(numOfCarPassing == 0 && cars[connector.from].empty()){
                        if(!cars[(connector.from+1)%4].empty()) {
                            pthread_cond_broadcast(&timed_waiter[(connector.from+1)%4]);
                            //std::cout << car.ID << " signalled " << connector.from+1 << std::endl;
                        }
                        else if(!cars[(connector.from+2)%4].empty()){ 
                            pthread_cond_broadcast(&timed_waiter[(connector.from+2)%4]);
                        }
                        else if(!cars[(connector.from+3)%4].empty()){ 
                            pthread_cond_broadcast(&timed_waiter[(connector.from+3)%4]);
                        }
                    } 
                    break;
                }
                else{
                    pthread_cond_wait(&road_condition[connector.from], &mut);
                }
            }
            else{
                int i=0;
                for( ; i<4; i++){
                    if(i == connector.from) continue;
                    if(!cars[i].empty()){
                        break;
                    }
                }
                if(i==4 && numOfCarPassing == 0){
                    curDirection = connector.from;
                } 
                
                else if(cars[connector.from].front() != car.ID){
                    pthread_cond_wait(&road_condition[connector.from], &mut);
                }
                else{ // the first car in another direction it should timed_wait
                    
                    
                    waiting_start:

                    auto now = std::chrono::system_clock::now();
                    auto maxWaitNano = std::chrono::nanoseconds(maximumWaitTime * 1000000);
                    auto waitUntil = now + maxWaitNano;
                    timespec wait_time;
                    wait_time.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(waitUntil.time_since_epoch()).count();
                    wait_time.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(waitUntil.time_since_epoch()).count() % 1000000000;
                    pthread_cond_timedwait(&timed_waiter[connector.from], &mut, &wait_time);
                    if(ramazan_cond){
                        int i = curDirection;
                        while(i != connector.from){
                            i = (i+1)%4;
                            if(cars[i].empty()){
                                continue;
                            }
                            else if(i == connector.from){
                                ramazan_cond = false;
                                break;
                            }
                            else{
                                pthread_cond_broadcast(&timed_waiter[i]);
                                goto waiting_start;
                            }
                        }
                    }
                    else goto waiting_start;
                    curDirection = connector.from;
                    if(numOfCarPassing > 0){
                        pthread_cond_wait(&noCarPassing, &mut);
                    }
                    ramazan_cond = true;
                    

                }
            }
        }
        pthread_mutex_unlock(&mut);
    }  // no need to unlock here, destructor of macro variable does it
};


// Variables to store input values
int numNarrowBridges, numFerries, numCrossroads, numCars;
std::vector<NarrowBridge> narrowBridges;
std::vector<Ferry> ferries;
std::vector<Crossroad> crossroads;
std::vector<Car> cars;


void* CarRoutine(void* args) {
    Car car = *(Car*)args;
    for(int i=0; i<car.path.size(); i++){
        PathObj connector = car.path[i];
        WriteOutput(car.ID, connector.connectorType, connector.connectorID, TRAVEL);
        sleep_milli(car.travelTime);
        switch (connector.connectorType)
        {
            case 'N':
                narrowBridges[connector.connectorID].pass(car, i);
                break;
            case 'F':
                ferries[connector.connectorID].pass(car, i);
                break;
            case 'C':
                crossroads[connector.connectorID].pass(car, i);
                break;
            default:
                break;
        }
    }
    return NULL;
}

int main() {
    std::cin >> numNarrowBridges;
    for (int i = 0; i < numNarrowBridges; ++i) {
        NarrowBridge props;
        props.ID = i;
        std::cin >> props.travelTime >> props.maximumWaitTime;
        narrowBridges.push_back(props);
    }

    std::cin >> numFerries;
    for (int i = 0; i < numFerries; ++i) {
        Ferry ferry = Ferry();
        std::cin >> ferry.travelTime >> ferry.maximumWaitTime >> ferry.capacity;
        ferries.push_back(ferry);
    }

    std::cin >> numCrossroads;
    for (int i = 0; i < numCrossroads; ++i) {
        Crossroad props;
        std::cin >> props.travelTime >> props.maximumWaitTime;
        crossroads.push_back(props);
    }

    std::cin >> numCars;
    pthread_t carThreads[numCars];

    for (int i = 0; i < numCars; ++i) {
        Car* car = new Car();
        car->ID = i;        
        std::cin >> car->travelTime >> car->pathLength;
        for (int j = 0; j < car->pathLength; ++j) {
            PathObj po;
            std::cin >> po.connectorType >> po.connectorID >> po.from >> po.to;
            car->path.push_back(po);
        }
        cars.push_back(*car);
        
    }
    InitWriteOutput();
    for(int i=0; i < numCars; i++){
        pthread_create(&carThreads[i], NULL, CarRoutine, (void*)&cars[i]);
    }
    
    for(auto &carThread : carThreads)
    {
        pthread_join(carThread, NULL);
    }

    return 0;
}

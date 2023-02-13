#include <iostream>
#include <fstream>
#include <pthread.h>
#include <queue>
#include <unordered_map>
#include <unordered_set>

pthread_barrier_t barrier;
pthread_mutex_t queueMutex;

// structure for map
// each map has nrReducers partial lists
typedef struct mapArg {
    std::queue<std::string> *fileQueue;
    int nrReducers;
    std::unordered_map<int, std::vector<int>> *map;
} mapArg;

// structure for reduce
// each reducer has nrMappers lists
typedef struct reducerArg {
    std::unordered_map<int, std::vector<int>> *maps;
    int nrMappers;
    int id;
} reducerArg;

// binary search algorithm for finding the perfect power 
// return 1 if there is a number powered that checks : powered^exp = n
// return 0 otherwise 
int find_power(int n, int exp) {
    int l = 1, r = n;
    long long number = n;
    int mid;
    while (l <= r) {
        mid = l + (r - l) / 2;
        long long powered = 1;
        for (int i = 0; i < exp; i++) {
            powered = powered * mid;
            if (powered > n) {
                break;
            }
        }
        if (powered < number) {
            l = mid + 1;
        }
        else if (powered > number) {
            r = mid - 1;
        } else {
            return 1;
        }
    }
    return 0;
}

// for each map thread, it's checking if the queue is empty
// then takes a file and process it
// the numbers are taken in order from the file and it is 
// checked for each value if it is a perfect power for each
// of the powers given by the reduction number
// the resulting numbers are saved in a map by each map thread
// which are then aggregated in the reduce threads
void *map(void *arg) {
    mapArg args = *(mapArg *) arg;
    for (int j = 2; j <= args.nrReducers + 1; j++) {
        std::vector<int> vec;
        args.map->insert(std::pair<int, std::vector<int>>(j, vec));
    }
    while (!args.fileQueue->empty()) {
        pthread_mutex_lock(&queueMutex);
        if (args.fileQueue->empty()) {
            pthread_mutex_unlock(&queueMutex);
            continue;
        }
        std::string currFile = args.fileQueue->front();
        args.fileQueue->pop();
        pthread_mutex_unlock(&queueMutex);
        std::ifstream in(currFile.data());
        int n;
        in >> n;
        for (int i = 0; i < n; i++) {
            int nr;
            in >> nr;
            for (int j = 2; j <= args.nrReducers + 1; j++) {
                if (find_power(nr, j)) {
                    args.map->at(j).push_back(nr);
                }
            }
        }
        in.close();
    }
    pthread_barrier_wait(&barrier);
    return NULL;
}

// reduce threads will keep track of the unique numbers
// and write the result in the corresponding file
void *reduce(void *arg) {
    reducerArg args = *(reducerArg *) arg;
    pthread_barrier_wait(&barrier);
    std::unordered_set<int> aggregate;
    for (int i = 0; i < args.nrMappers; i++) {
        for (unsigned int j = 0; j < args.maps[i][args.id].size(); j++) {
            aggregate.insert(args.maps[i][args.id][j]);
        }
    }
    char output[11];
    sprintf(output, "out%d.txt", args.id);
    std::ofstream out(output);
    out << aggregate.size();
    out.close();
    return NULL;
}

int main(int argc, char **argv) {

    if(argc < 4) {
		std::cout << "Insufficient number of parameters: ./tema1 m r inputFile" << "\n";
		return 1;
	}

    // convert to integer numberMappers and numberReducers
    int m = atoi(argv[1]);
    int r = atoi(argv[2]);

    // queue for input files
    std::queue<std::string> fileQueue;

    // open the file input 
    // read from file number of files to process
    std::ifstream in(argv[3]);
    int nrFiles;
    in >> nrFiles;

    // read files to process 
    // insert them in the queue
    for (int i = 0; i < nrFiles; i++) {
        std::string file;
        in >> file;
        fileQueue.push(file);
    }
    in.close();

    std::unordered_map<int, std::vector<int>> maps[m];
    int err;
    pthread_barrier_init(&barrier, NULL, m + r);
    pthread_mutex_init(&queueMutex, NULL);
    pthread_t threads[m + r];
    mapArg args[m];
    reducerArg argsR[r];

    for (int i = 0; i < m + r; i++) {
        if (i < m) {
            // create map threads
            args[i].map = &maps[i];
            args[i].fileQueue = &fileQueue;
            args[i].nrReducers = r;
            err = pthread_create(&threads[i], NULL, map, &args[i]);
        } else {
            // create reduce threads
            argsR[i - m].maps = maps;
            argsR[i - m].id = i - m + 2;
            argsR[i - m].nrMappers = m;
            err = pthread_create(&threads[i], NULL, reduce, &argsR[i - m]);
        }
        if (err) {
            std::cout << "Error creating thread " << i << "\n";
            return 1;
        }
    }

    // join threads map and reduce
    for (int i = 0; i < m + r; i++) {
        err = pthread_join(threads[i], NULL);
        if (err) {
            std::cout << "Error waiting for thread " << i << "\n";
            return 1;
        }
    }

    err = pthread_mutex_destroy(&queueMutex);
    if (err) {
        std::cout << "Error destroying the mutex" << "\n";
        return 1;
    }

    err = pthread_barrier_destroy(&barrier);
    if (err) {
        std::cout << "Error destroying the barrier" << "\n";
        return 1;
    }

    return 0;
}
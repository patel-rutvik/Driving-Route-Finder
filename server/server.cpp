/*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
Assignment 2 Part 2: Driving Route Finder
Names: Rutvik Patel, Kaden Dreger
ID: 1530012, 1528632
CCID: rutvik, kaden
CMPUT 275 Winter 2018

This program demonstrates an implementation of reading in from a CSV file,
constructing a weighted digraph, and process requests from the user. The user
can specify a latitude and longitude of both start and end points and this
program will find the shortest path and print out the waypoints along the way.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
#include <string>
#include <iostream>
#include <fstream>
#include "dijkstra.h"
#include "digraph.h"
#include "wdigraph.h"
#include "serialport.h"
#include <sstream>
#include <cassert>


SerialPort port("/dev/ttyACM0");
struct Point {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
The Point struct stores the latitude and longitude of the points to be read in.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    ll lat;  // latitude of the point
    ll lon;  // longitude of the point
};


ll manhattan(const Point& pt1, const Point& pt2) {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
The manhattan function takes the parameters:
    pt1: the first/starting point
    pt2: the second/end point

It returns the parameters:
    dist: the manhattan distance between the two points

The point of this function is to calculate the manhattan distance between the
two points passed into the function.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    // calculate manhattan distance
    ll dist = abs(pt1.lat - pt2.lat) + abs(pt1.lon - pt2.lon);
    return dist;
}

// This is a modified implementation of split from:
// https://www.fluentcpp.com/2017/04/21/how-to-split-a-string-in-c/
vector<string> split(string str, char delim) {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
The split function takes the parameters:
    str: the string to be split
    delim: the delimiter for splitting

It returns the parameters:
    vector<string>: a vector containing the string split up

The point of this function is to split the given string by the given delimiter
abd return it as a vector.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    vector<string> temp;
    stringstream ss(str);
    string token;
    while (getline(ss, token, delim)) {
        temp.push_back(token);
    }
    return temp;
}


void readGraph(string filename, WDigraph& graph,
    unordered_map<int, Point>& points) {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
The readGraph function takes the parameters:
    filename: name of the CSV file describing the road network
    graph   : an instance of the Wdigraph class
    points  : a mapping b/w vertex ID's and their respective coordinates

It returns the parameters:
    dist: the manhattan distance between the two points

The point of this function is to calculate the manhattan distance between the
two points passed into the function.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    /* method to open and read from files found from:
    cplusplus.com/doc/tutorial/files/             */
    ifstream file;  // file object
    string token;
    char delim = ',';  // the delimeter of choice
    int ID;
    long double coord;
    Point p, p1, p2;
    file.open(filename);  // opening the file
    if (file.is_open()) {  // if the file is open...
        while (getline(file, token, delim)) {
            if (token == "V") {
                getline(file, token, delim);
                ID = stoi(token);
                getline(file, token, delim);
                coord = stold(token) * 100000;
                coord = static_cast<ll>(coord);
                p.lat = coord;  // saving the coordinate to the points
                getline(file, token);
                coord = stold(token) * 100000;
                coord = static_cast<ll>(coord);
                p.lon = coord;  // saving the coordinate to the points
                points[ID] = p;  // insert into the map
            } else if (token == "E") {
                getline(file, token, delim);
                int id1 = stoi(token);
                getline(file, token, delim);
                int id2 = stoi(token);
                getline(file, token);
                string name = token;
                p1 = points[id1];  // get respective point
                p2 = points[id2];  // get respective point
                // calc man dist, pass in saved points
                ll weight = manhattan(p1, p2);  // calculate distance
                graph.addEdge(id1, id2, weight);  // add the weighted edge
            }
        }
    } else {
        /*Error message in case the file is not found. */
        cout << "ERROR. Unable to open the file " << filename << "." << endl;
    }
    file.close();  // closing the file
}


int closestVert(const ll& lat, const ll& lon, unordered_map<int, Point>& p) {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
The closestVert function takes the parameters:
    lat   : latitude of a given point
    lon   : longitude of a given point
    p     : a mapping b/w vertex ID's and their respective coordinates

It returns the parameters:
    vert  : the resulting vertex with the given lat and lon values

The point of this function is to find the vertex in our point map based on the
passed in lat and lon values.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    Point point;
    ll dist, temp;
    point.lat = lat;  // assign lat parameter to struct
    point.lon = lon;  // assign lon parameter to struct
    int vert = p.begin()->first;  // set starting vertex to the first one
    dist = manhattan(p[vert], point);  // initialize the distance
    for (auto i : p) {
        temp = manhattan(i.second, point);  // calculate temp distance
        if (temp < dist) {  // if that distance is less than previous...
            vert = i.first;  // set the vertex to the current iteration
            dist = temp;  // set the distance to the new lower value
        }
    }
    return vert;  // return resulting vertex
}


bool printWaypoints(unordered_map<int, Point>& p, unordered_map<int, PLI>& tree,
                    int& startVert, int& endVert) {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
The printWaypoints function takes the parameters:
    tree     : the search tree with respective to the starting vertex
    startVert: starting vertex
    endVert  : end vertex
    p        : a mapping b/w vertex ID's and their respective coordinates

It does not return any parameters.

The point of this function is to print the number of waypoints, along with their
lat and lon values, enroute to the end vertex.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    stack<int> route;
    int vert = endVert;  // starting at the end point
    string ack, temp;
    while (route.top() != startVert) {  // while we have not reached the start
        route.push(vert);  // push the vertex onto the stack
        vert = tree[vert].second;  // set the vertex to the parent of current
    }
    if (route.size() > 500) {
        port.writeline("N 0\n");
        return false;
    }
    cout << "N " << route.size() << endl;  // print out number of waypoints
    port.writeline("N ");
    temp = to_string(route.size());
    port.writeline(temp);
    port.writeline("\n");  // write line does not include new line character
    int size = route.size();
    for (int i = 0; i < size; i++) {
        // receive acknowledgement
        ack = port.readline(1000);
        cout << "ack received!" << endl;
        if (ack[0] == 'A') {
            cout << endl;
            /*print out the waypoint coordinates*/
            cout << "W " << p[route.top()].lat << " " << p[route.top()].lon;
            cout << endl;
            assert(port.writeline("W "));
            temp = to_string(p[route.top()].lat);
            assert(port.writeline(temp));
            assert(port.writeline(" "));
            temp = to_string(p[route.top()].lon);
            assert(port.writeline(temp));
            assert(port.writeline("\n"));
            route.pop();  // removing the element from the stack
        } else {
            // timeout...
            cout << "timeout.." << endl << endl;
            return true;
        }
    }
    // receive acknowledgement
    ack = port.readline(1000);
    if (ack == "") {
        return true;
    }
    if (ack[0] == 'A') {
        port.writeline("E\n");
    } else {
        return true;
    }
    // indicating end of request
    return false;  // no timeout
}


void processRequest(WDigraph graph, unordered_map<int, Point> points) {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
The processRequest function takes the parameters:
    graph: the Digraph created from the csv file
    points: the points for each vertex

It returns no parameters.

The point of this function is to process the communication with the Arduino
by sending the number of waypoints and the waypoints themselves.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    bool timeout = false;
    string temp = port.readline(0);
    if (temp[0] == 'R') {
        vector<string> request = split(temp, ' ');  // find citation later...
        cout << "reading coordinates..." << endl;
        cout << endl;
        ll startLat, startLon, endLat, endLon;
        // read in the coordinates
        startLat = stoll(request[1]);
        startLon = stoll(request[2]);
        endLat = stoll(request[3]);
        endLon = stoll(request[4]);
        int start = closestVert(startLat, startLon, points);  // map to vertex
        int end = closestVert(endLat, endLon, points);  // map to vertex
        unordered_map<int, PLI> heapTree;
        dijkstra(graph, start, heapTree);
        if (heapTree.find(end) == heapTree.end()) {
            /* handling the 0 case */
            cout << "N 0" << endl;
            assert(port.writeline("N 0\n"));
        } else {
            /* print out the waypoints enroute to the destination */
            timeout = printWaypoints(points, heapTree, start, end);
            if (!timeout) {
                cout << "sent waypoints to client" << endl << endl;
            } else {
                cout << "failed to send waypoints" << endl << endl;
            }
        }
    }
}


int main() {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
This is the main function of this program, it calls our implementations above,
along with handling some of the input and output functionality.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    WDigraph graph;  // creating instance of Wdigraph
    unordered_map<int, Point> points;
    readGraph("edmonton-roads-2.0.1.txt", graph, points);
    cout << endl << "graph constructed" << endl;
    while (true) {
        processRequest(graph, points);
    }
    return 0;
}

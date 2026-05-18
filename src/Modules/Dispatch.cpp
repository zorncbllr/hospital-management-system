#include <hms/Core/Exceptions.h>
#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>
#include <hms/Modules/Dispatch.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace hms {


DispatchModule::ZoneGraph DispatchModule::loadGraph(const std::string& path) {
    ZoneGraph graph;
    std::ifstream in(path);
    if (!in) {
        throw FileException("could not open zone graph at " + path);
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream tokens(line);
        std::string keyword;
        tokens >> keyword;
        if (keyword == "NODES") {
            std::string rest;
            std::getline(tokens, rest);
            std::string current;
            for (char c : rest) {
                if (c == ',' || c == ' ' || c == '\t') {
                    if (!current.empty()) {
                        graph.nodeIndex[current] = static_cast<int>(graph.nodes.size());
                        graph.nodes.push_back(current);
                        current.clear();
                    }
                } else {
                    current += c;
                }
            }
            if (!current.empty()) {
                graph.nodeIndex[current] = static_cast<int>(graph.nodes.size());
                graph.nodes.push_back(current);
            }
            graph.adjacency.assign(graph.nodes.size(), {});
        } else if (keyword == "EDGE") {
            std::string from, to;
            int weight = 0;
            tokens >> from >> to >> weight;
            if (weight <= 0) {
                throw FileException("graph edge weight must be positive: "
                                    + from + "->" + to + " weight=" + std::to_string(weight));
            }
            auto fromIt = graph.nodeIndex.find(from);
            auto toIt = graph.nodeIndex.find(to);
            if (fromIt == graph.nodeIndex.end() ||
                toIt == graph.nodeIndex.end()) {
                throw FileException("graph edge references unknown node: "
                                    + from + "->" + to);
            }
            graph.adjacency[fromIt->second].push_back({toIt->second, weight});
            graph.adjacency[toIt->second].push_back({fromIt->second, weight});
        }
    }
    if (graph.nodes.empty()) {
        throw FileException("graph file has no nodes");
    }
    return graph;
}

DispatchModule::DijkstraResult DispatchModule::dijkstra(const ZoneGraph& graph, int source) {
    const int n = static_cast<int>(graph.nodes.size());
    DijkstraResult result;
    result.distance.assign(n, std::numeric_limits<int>::max());
    result.previous.assign(n, -1);
    result.distance[source] = 0;

    using NodeDistance = std::pair<int, int>;
    std::priority_queue<
        NodeDistance,
        std::vector<NodeDistance>,
        std::greater<NodeDistance>> queue;
    queue.push({0, source});

    while (!queue.empty()) {
        auto [currentDistance, node] = queue.top();
        queue.pop();
        if (currentDistance > result.distance[node]) continue;
        for (const auto& [neighbor, weight] : graph.adjacency[node]) {
            int candidate = currentDistance + weight;
            if (candidate < result.distance[neighbor]) {
                result.distance[neighbor] = candidate;
                result.previous[neighbor] = node;
                queue.push({candidate, neighbor});
            }
        }
    }
    return result;
}

std::vector<int> DispatchModule::reconstructPath(const DijkstraResult& result, int target) {
    std::vector<int> path;
    if (result.distance[target] == std::numeric_limits<int>::max())
        return path;
    for (int node = target; node != -1; node = result.previous[node]) {
        path.push_back(node);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

void DispatchModule::showZones(const ZoneGraph& graph) {
    std::vector<std::string> zHeaders{"#", "Zone", "Note"};
    std::vector<std::vector<std::string>> zRows;
    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        zRows.push_back({
            std::to_string(i),
            graph.nodes[i],
            i == 0 ? "Ambulance station" : "",
        });
    }
    int zw = tui_.bannerOpen("AVAILABLE ZONES", "",
                             {"Home", "Ambulance Dispatch", "Request"},
                             tui_.tableBoxWidth(zHeaders, zRows));
    tui_.tableInBox(zw, zHeaders, zRows);
}

void DispatchModule::requestAmbulance() {
    ZoneGraph graph = loadGraph(hospital_.graphPath());

    tui_.clearScreen();
    showZones(graph);

    int target = validation_.readInt(
        "Destination zone number",
        0, static_cast<int>(graph.nodes.size()) - 1);

    DijkstraResult result = dijkstra(graph, 0);
    if (result.distance[target] == std::numeric_limits<int>::max()) {
        throw NotFoundException(
            "no route to " + graph.nodes[target]);
    }
    std::vector<int> path = reconstructPath(result, target);

    std::ostringstream pathStream;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i > 0) pathStream << " → ";
        pathStream << graph.nodes[path[i]];
    }

    std::vector<std::string> rHeaders{"Field", "Value"};
    std::vector<std::vector<std::string>> rRows{
        {"From", graph.nodes[0]},
        {"To", graph.nodes[target]},
        {"ETA", std::to_string(result.distance[target]) + " minutes"},
        {"Path", pathStream.str()},
    };
    int rw = tui_.bannerOpen("ROUTE PLAN", "",
                             {"Home", "Ambulance Dispatch", "Request"},
                             tui_.tableBoxWidth(rHeaders, rRows));
    tui_.tableInBox(rw, rHeaders, rRows);
    std::cout << "\n";
    tui_.pause();
}

void DispatchModule::allDistances() {
    ZoneGraph graph = loadGraph(hospital_.graphPath());
    tui_.clearScreen();

    DijkstraResult result = dijkstra(graph, 0);
    std::vector<std::string> headers{ "Zone", "ETA", "Route" };
    std::vector<std::vector<std::string>> rows;
    for (std::size_t i = 1; i < graph.nodes.size(); ++i) {
        std::string distanceCell = (result.distance[i] == std::numeric_limits<int>::max())
            ? "unreachable"
            : std::to_string(result.distance[i]) + " min";
        std::vector<int> path = reconstructPath(result, static_cast<int>(i));
        std::ostringstream pathStream;
        for (std::size_t j = 0; j < path.size(); ++j) {
            if (j > 0) pathStream << " → ";
            pathStream << graph.nodes[path[j]];
        }
        rows.push_back({ graph.nodes[i], distanceCell, pathStream.str() });
    }
    int bw = tui_.bannerOpen("DISTANCES FROM STATION", "fastest route to every zone",
                             {"Home", "Ambulance Dispatch", "All Distances"},
                             tui_.tableBoxWidth(headers, rows, "Source: " + graph.nodes[0]));
    tui_.tableInBox(bw, headers, rows, "Source: " + graph.nodes[0]);
    std::cout << "\n";
    tui_.pause();
}

void DispatchModule::showGraph() {
    ZoneGraph graph = loadGraph(hospital_.graphPath());

    tui_.clearScreen();
    tui_.banner("ZONE GRAPH", "nodes and edges", {"Home", "Ambulance Dispatch", "View Graph"});
    std::cout << "\n";

    showZones(graph);
    std::cout << "\n";

    std::vector<std::string> eHeaders{"From", "To", "Travel Time"};
    std::vector<std::vector<std::string>> eRows;
    for (std::size_t i = 0; i < graph.adjacency.size(); ++i) {
        for (const auto& [neighbor, weight] : graph.adjacency[i]) {
            if (static_cast<std::size_t>(neighbor) < i) continue;
            eRows.push_back({
                graph.nodes[i],
                graph.nodes[neighbor],
                std::to_string(weight) + " min",
            });
        }
    }
    int ew = tui_.bannerOpen("EDGES", "",
                             {"Home", "Ambulance Dispatch", "View Graph"},
                             tui_.tableBoxWidth(eHeaders, eRows));
    tui_.tableInBox(ew, eHeaders, eRows);
    std::cout << "\n";
    tui_.pause();
}


void DispatchModule::run() {
    while (true) {
        char choice = tui_.menu(
            "AMBULANCE DISPATCH",
            {"Home", "Ambulance Dispatch"},
            {
                { '1', "Request ambulance",  "fastest route to one zone" },
                { '2', "All zone distances", "ETA to every zone" },
                { '3', "Show zone graph",    "zones and connections" },
                { 'B', "Back",               "return to main menu" },
            });
        switch (choice) {
            case '1': requestAmbulance(); break;
            case '2': allDistances();     break;
            case '3': showGraph();        break;
            case 'B': return;
        }
    }
}

}

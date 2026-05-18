#ifndef HMS_MODULES_DISPATCH_H
#define HMS_MODULES_DISPATCH_H

#include <hms/Modules/Module.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hms {

class DispatchModule : public Module {
public:
    using Module::Module;

    void run() override;
    char        menuKey()   const override { return '8'; }
    std::string menuLabel() const override { return "Ambulance Dispatch"; }
    std::string menuHint()  const override { return "emergency routing"; }

private:
    struct ZoneGraph {
        std::vector<std::string> nodes;
        std::unordered_map<std::string, int> nodeIndex;
        std::vector<std::vector<std::pair<int, int>>> adjacency;
    };
    struct DijkstraResult {
        std::vector<int> distance;
        std::vector<int> previous;
    };

    static ZoneGraph        loadGraph(const std::string& path);
    static DijkstraResult   dijkstra(const ZoneGraph& graph, int source);
    static std::vector<int> reconstructPath(const DijkstraResult& result, int target);

    void showZones(const ZoneGraph& graph);
    void requestAmbulance();
    void allDistances();
    void showGraph();
};

}

#endif

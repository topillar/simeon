#include <algorithm>
#include <RMSA/Route.h>
#include <Structure/Slot.h>
#include <Structure/Node.h>
#include <Structure/Link.h>

using namespace RMSA;

Route::Route(std::vector<TransparentSegment> Segments,
             std::map<std::weak_ptr<Link>, std::vector<std::weak_ptr<Slot>>, std::owner_less<std::weak_ptr<Link>>>
             Slots) : Segments(Segments), Slots(Slots)
{
    Nodes.clear();
    Links.clear();
    Regenerators.clear();

    for (auto &segment : Segments)
        {
        Nodes.insert(Nodes.end(), segment.Nodes.begin(), segment.Nodes.end() - 1);
#ifdef RUN_ASSERTIONS
        if (segment.NumRegUsed && (segment.Nodes.back().lock()->get_NodeType() == Node::TransparentNode))
            {
            std::cerr << "Trying to regenerate in transparent node." << std::endl;
            abort();
            }
#endif
        Regenerators.emplace(segment.Nodes.back(), segment.NumRegUsed);

        Links.insert(Links.end(), segment.Links.begin(), segment.Links.end());
        }

    if (!Segments.empty())
        {
        Nodes.push_back(Segments.back().Nodes.back());
        }
}

Route::Route(const Route &route)
{
    for (auto &segment : route.Segments)
        {
        Segments.push_back(segment);
        }

    for (auto &node : route.Nodes)
        {
        Nodes.push_back(node);
        }

    for (auto &link : route.Links)
        {
        Links.push_back(link);
        }

    for (auto &node : route.Regenerators)
        {
        Regenerators.emplace(node.first, node.second);
        }

    for (auto &link : route.Slots)
        {
        Slots.emplace(link.first, link.second);
        }
}

Signal Route::bypass(Signal S)
{
    return Segments.back().bypass(S);
}

Signal &Route::partial_bypass(Signal &S, std::weak_ptr<Node> orig,
                              std::weak_ptr<Node> dest)
{

    auto currentNode = Nodes.begin();

    for (auto node = Nodes.begin(); node != Nodes.end(); ++node)
        {
#ifdef RUN_ASSERTIONS
        if (node->lock() == Nodes.end()->lock())
            {
            std::cerr << "Source node is not in the Route." << std::endl;
            abort();
            }
#endif
        if (orig.lock() == (*node).lock())
            {
            currentNode = node;
            break;
            }
        }

    for (auto node = currentNode ; node != Nodes.end(); ++node)
        {
#ifdef RUN_ASSERTIONS
        if (node->lock() == Nodes.end()->lock())
            {
            std::cerr << "Destination node is not in the Route." << std::endl;
            abort();
            }
#endif
        if (dest.lock() == (*node).lock())
            {
            break;
            }
        }

    auto currentLink = Links.begin();

    while ((*currentLink).lock()->Origin.lock() != orig.lock())
        {
        currentLink++;
        }

    //Traverses the network
    do
        {
        if ((*currentNode).lock() == orig.lock())
            {
            S = orig.lock()->add(S);
            }
        else if ((*currentNode).lock() == dest.lock())
            {
            S = dest.lock()->drop(S);
            break;
            }
        else
            {
            S = (*currentNode).lock()->bypass(S);
            }

        currentNode++;

        S = (*currentLink).lock()->bypass(S);

        currentLink++;
        }
    while (currentNode != Nodes.end());

    return S;
}

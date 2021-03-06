#include <GeneralPurposeAlgorithms/NSGA-II/NSGA2_Generation.h>
#include <GeneralPurposeAlgorithms/NSGA-II/NSGA2_Individual.h>
#include <GeneralPurposeAlgorithms/NSGA-II/NSGA2_Parameter.h>
#include <GeneralPurposeAlgorithms/NSGA-II/NSGA2.h>
#include <GeneralClasses/RandomGenerator.h>
#include <algorithm>
#include <limits>
#include <iostream>
#include <fstream>

using namespace NSGA_II;

NSGA2_Generation::NSGA2_Generation() : isEvaluated(false)
{

}

void NSGA2_Generation::eval()
{
    #pragma omp parallel for ordered schedule(dynamic)
    for (size_t i = 0; i < people.size(); i++)
        {
        people[i]->eval();
        }

    evalParetoFront();
    evalCrowdingDistances();

    isEvaluated = true;
}

void NSGA2_Generation::evalCrowdingDistances(int ParetoFront)
{
    if (isEvaluated)
        {
        return;
        }

    auto front = getParetoFront(ParetoFront);
    unsigned int numParameters = people.front()->getNumParameters();

    //Initialize crowding distances
    for (auto &individual : front)
        {
        individual->crowdingDistance = 0;
        }

    for (size_t par = 0; par < numParameters; par++)
        {
        struct Param
            {
            Param(std::shared_ptr<NSGA2_Individual> individual, double value) :
                individual(individual), value(value) {}
            std::shared_ptr<NSGA2_Individual> individual;
            double value;
            bool operator < (const Param &other) const
                {
                return value < other.value;
                }
            };

        std::vector<Param> IndivList;

        for (auto indiv : front)
            {
            IndivList.push_back(Param(indiv, indiv->getParameterValue(par)));
            }

        std::sort(IndivList.begin(), IndivList.end());

        double minValue = IndivList.front().value;
        double maxValue = IndivList.back().value;

        //Extreme values have infinite crowding distance
        IndivList.front().individual->crowdingDistance =
            std::numeric_limits<double>::max();
        IndivList.back().individual->crowdingDistance =
            std::numeric_limits<double>::max();

        for (size_t i = 1; i < IndivList.size() - 1; i++)
            {
            if (IndivList[i].individual->crowdingDistance ==
                    std::numeric_limits<double>::max())
                {
                continue;
                }

            IndivList[i].individual->crowdingDistance +=
                (IndivList[i + 1].value - IndivList[i - 1].value) / (maxValue - minValue);
            }
        }
}

void NSGA2_Generation::evalCrowdingDistances()
{
    if (isEvaluated)
        {
        return;
        }

    int paretoFront = 1;

    while (!getParetoFront(paretoFront).empty())
        {
        evalCrowdingDistances(paretoFront++);
        }
}

void NSGA2_Generation::evalParetoFront()
{
    if (isEvaluated)
        {
        return;
        }

    int numNotInParetoFront = 0;
    int currentParetoFront = 1;

    for (auto &indiv : people)
        {
        indiv->crowdingDistance = indiv->paretoFront = -1;
        }

    do
        {
        std::vector<std::shared_ptr<NSGA2_Individual>> currentFront;
        numNotInParetoFront = 0;

        for (auto &indiv : people)
            {
            if (indiv->paretoFront != -1)
                {
                continue;
                }

            bool isDominated = false;

            for (auto &other : people)
                {
                if ((other->paretoFront != -1) || isDominated)
                    {
                    continue;
                    }
                isDominated = isDominated || (indiv->isDominatedBy(other));
                }

            if (!isDominated)
                {
                currentFront.push_back(indiv);
                continue;
                }

            numNotInParetoFront++;
            }

        for (auto &indiv : currentFront)
            {
            indiv->paretoFront = currentParetoFront;
            }

        currentParetoFront++;
        }
    while (numNotInParetoFront != 0);
}

void NSGA2_Generation::operator += (std::shared_ptr<NSGA2_Individual> other)
{
    isEvaluated = false;

    auto indiv = other->clone();
    indiv->paretoFront = indiv->crowdingDistance = -1;
    people.push_back(indiv);
}

void NSGA2_Generation::operator +=(std::shared_ptr<NSGA2_Generation> other)
{
    for (auto &individual : other->people)
        {
        this->operator +=(individual);
        }
}

std::vector<std::shared_ptr<NSGA2_Individual>>
        NSGA2_Generation::getParetoFront(int i)
{
    std::vector<std::shared_ptr<NSGA2_Individual>> ParetoFront;

    for (auto &indiv : people)
        {
        if (indiv->paretoFront == -1)
            {
            evalParetoFront();
            }

        if (indiv->paretoFront == i)
            {
            ParetoFront.push_back(indiv);
            }
        }

    return ParetoFront;
}

void NSGA2_Generation::print(std::string filename, int paretoFront)
{
    auto front = getParetoFront(paretoFront);

    if (filename == "NO_FILE_GIVEN")
        {
        //Prints at most five elements
        size_t numElements = (front.size() < 5 ? front.size() : 5);

        for (size_t indiv = 0; indiv < numElements; indiv++)
            {
            front[indiv]->print();
            }

        if (numElements < front.size())
            {
            std::cout << "[other " << front.size() - numElements << " elements suppressed]"
                      << std::endl;
            }
        }
    else
        {
        std::ofstream OutFile(filename.c_str());

        //Header: Parameter Names
        OutFile << "# ";
        for (unsigned par = 0; par < front.front()->getNumParameters(); par++)
            {
            OutFile << front.front()->getParameter(par)->get_ParamName() << " --- ";
            }
        OutFile << std::endl;

        //Prints the Parameters
        for (auto &indiv : front)
            {
            OutFile << indiv->print(false) << std::endl;
            }

        OutFile.close();
        }
}

void NSGA2_Generation::breed(
    unsigned int a, unsigned int b, NSGA2_Generation &dest)
{
    if (a == b)
        {
        return;
        }

    auto iterator_a = people.begin() + a;
    auto iterator_b = people.begin() + b;

    std::uniform_real_distribution<double> dist(0, 1);
    std::vector<int> GeneA = people[a]->getGenes(), GeneB = people[b]->getGenes();

    if (dist(random_generator) < NSGA2::breedingProb)   //breeds
        {
        for (size_t i = 0; i < GeneA.size(); ++i)
            {
            if (dist(random_generator) < 0.5)
                {
                std::swap(GeneA[i], GeneB[i]); //swaps genes of the individuals
                }
            }
        }

    auto newIndivA = people[a]->clone(), newIndivB = people[b]->clone();
    newIndivA->setGene(GeneA);
    newIndivB->setGene(GeneB);
    dest += newIndivA;
    dest += newIndivB;

    people.erase(iterator_a);
    people.erase(iterator_b);
}

std::shared_ptr<NSGA2_Individual> NSGA2_Generation::binaryTournament()
{
    std::uniform_int_distribution<> dist(0, people.size() - 1);

    //selects random Individual
    auto individual = people.begin() + dist(random_generator);

    //selects binaryTournamentParameter other individuals to "duel" with
    //individual, in terms of Pareto Front.
    for (unsigned selec = 0; selec < NSGA2::binaryTournamentParameter; selec++)
        {
        auto adversary = people.begin() + dist(random_generator);

        while (adversary == individual)
            {
            adversary = people.begin() + dist(random_generator);
            }

        if ((*adversary)->paretoFront < (*individual)->paretoFront)
            {
            individual = adversary;
            }
        }

    return *individual;
}

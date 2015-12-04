//**************************************************************
//	tiny bandit test suite for Steiner Tree algorithms
//
//	Author: Tilo Wiedera
//**************************************************************

#include <string>
#include <tuple>
#include <vector>
#include <bandit/bandit.h>
#include <ogdf/fileformats/GraphIO.h>
#include <ogdf/basic/List.h>
#include <ogdf/graphalg/MinSteinerTreeDirectedCut.h>
#include <ogdf/graphalg/MaxFlowEdmondsKarp.h>
#include <ogdf/graphalg/MaxFlowGoldbergTarjan.h>
#include <ogdf/graphalg/MinSteinerTreeKou.h>
#include <ogdf/graphalg/MinSteinerTreeMehlhorn.h>
#include <ogdf/graphalg/MinSteinerTreeTakahashi.h>
#include <ogdf/graphalg/MinSteinerTreeRZLoss.h>
#include <ogdf/graphalg/MinSteinerTreeZelikovsky.h>
#include <ogdf/graphalg/MinSteinerTreeShore.h>
#include <ogdf/graphalg/MinSteinerTreePrimalDual.h>
#include <ogdf/graphalg/MinSteinerTreeDualAscent.h>
#include <ogdf/graphalg/MinSteinerTreeGoemans139.h>
#include <resources.h>

using namespace ogdf;
using namespace bandit;

template<typename T> using ModuleTuple = std::tuple<std::string, MinSteinerTreeModule<T>*, int, double>;

// Test MinSteinerTreeModule<T>::isSteinerTree()
template<typename T>
static void testIsSteinerTree()
{
	EdgeWeightedGraph<T> graph;
	EdgeWeightedGraphCopy<T> *tree;
	NodeArray<bool> isTerminal;
	List<node> terminals;

	before_each([&]() {
		node v0 = graph.newNode();
		node v1 = graph.newNode();
		node v2 = graph.newNode();
		graph.newEdge(v0, v1, 2);
		graph.newEdge(v1, v2, 3);
		edge eCycle = graph.newEdge(v2, v0, 7);

		isTerminal.init(graph, false);
		isTerminal[v0] = isTerminal[v2] = true;

		for (auto v : graph.nodes) {
			if (isTerminal[v]) {
				terminals.pushBack(v);
			}
		}

		tree = new EdgeWeightedGraphCopy<T>(graph);
		tree->delEdge(tree->copy(eCycle));
	});

	after_each([&]() {
		delete tree;
		graph.clear();
		terminals.clear();
	});

	it("recognizes a valid Steiner tree", [&]() {
		AssertThat(MinSteinerTreeModule<T>::isSteinerTree(graph, terminals, isTerminal, *tree), IsTrue());
	});

	it("recognizes a disconnected Steiner tree", [&]() {
		tree->delEdge(tree->chooseEdge());

		AssertThat(MinSteinerTreeModule<T>::isSteinerTree(graph, terminals, isTerminal, *tree), IsFalse());
	});

	it("recognizes a Steiner tree with extra nodes", [&]() {
		node v = graph.newNode();
		isTerminal[v] = true;
		terminals.pushFront(v);

		AssertThat(MinSteinerTreeModule<T>::isSteinerTree(graph, terminals, isTerminal, *tree), IsFalse());
	});

	it("recognizes a Steiner tree with redundant Steiner node", [&]() {
		node u = terminals.front();
		node v = graph.newNode();
		edge e = graph.newEdge(u, v, 1);
		tree->newNode(v);
		tree->newEdge(e, 1);

		AssertThat(MinSteinerTreeModule<T>::isSteinerTree(graph, terminals, isTerminal, *tree), IsFalse());
	});
}

/**
 * Generates a new graph with an optimal Steiner tree.
 * Only very basic graphs are generated
 * to guarantee the optimality of the resulting Steiner tree.
 *
 * \param n
 *	number of nodes
 * \param graph
 *	the resulting graph
 * \param terminals
 *	this list will hold all terminals
 * \param isTerminal
 *	stores which node is a terminal
 * \param tree
 *	an optimal Steiner tree for this graph.
 */
template<typename T>
T randomOptimalSteiner(
  int n,
  EdgeWeightedGraph<T> &graph,
  List<node> &terminals,
  NodeArray<bool> &isTerminal,
  EdgeWeightedGraphCopy<T> &tree
)
{
	OGDF_ASSERT(n >= 0);

	T result = 0;

	graph.clear();
	terminals.clear();
	tree.clear();
	tree.createEmpty(graph);
	isTerminal.init(graph, false);

	if(n > 0) {
		node source = graph.newNode();
		tree.newNode(source);
		isTerminal[source] = true;
		terminals.pushFront(source);

		if(n > 1) {
			int numberOfAdditionalNodes = randomNumber(0, n-2);
			int numberOfNodes = n - numberOfAdditionalNodes;
			int numberOfEdges = randomNumber(numberOfNodes-1 + numberOfAdditionalNodes*2, (n*(n-1))/2);

			for(int i = 1; i < numberOfNodes; i++) {
				node v = graph.chooseNode();
				node u = graph.newNode();
				tree.newNode(u);

				edge e = graph.newEdge(v, u, 1);
				result++;
				tree.newEdge(e, 1);
				if(isTerminal[v] && v != source) {
					isTerminal[v] = false;
					terminals.del(terminals.search(v));
				}

				isTerminal[u] = true;
				terminals.pushFront(u);
			}

			for(int i = numberOfNodes-1; i < numberOfEdges;) {
				node v = graph.chooseNode();
				node u = graph.chooseNode();
				while(u == v) { u = graph.chooseNode(); }

				if(numberOfAdditionalNodes > 0) {
					node w = graph.newNode();
					graph.newEdge(v, w, n);
					graph.newEdge(w, u, n);
					numberOfAdditionalNodes--;
					i += 2;
				}
				else {
					if (graph.searchEdge(v, u) == nullptr
					 && graph.searchEdge(u, v) == nullptr) {
						graph.newEdge(v, u, n);
						i++;
					}
				}
			}
			OGDF_ASSERT(graph.numberOfEdges() == numberOfEdges);
			OGDF_ASSERT(tree.numberOfNodes() == numberOfNodes);
			OGDF_ASSERT(tree.numberOfEdges() == numberOfNodes - 1);
		}
	}
	OGDF_ASSERT(graph.numberOfNodes() == n);
	OGDF_ASSERT(isSimpleUndirected(graph));
	OGDF_ASSERT(isConnected(graph));
	return result;
}

/**
 * Test if modules generates a valid Steiner tree for a graph with given number of nodes
 */
template<typename T>
void testModuleOnRandomGraph(MinSteinerTreeModule<T> &alg, int n, double factor)
{
	it(string("generates a valid Steiner tree for a graph of " + to_string(n) + " nodes"), [&](){
		EdgeWeightedGraph<T> graph;
		EdgeWeightedGraphCopy<T> tree;
		NodeArray<bool> isTerminal(graph, false);
		List<node> terminals;

		T cost = randomOptimalSteiner<T>(n, graph, terminals, isTerminal, tree);
		cout << endl << "        graph has " << terminals.size() << " terminals" << endl;

		EdgeWeightedGraphCopy<T> *algTree;
		T algCost = alg.call(graph, terminals, isTerminal, algTree);

		AssertThat(MinSteinerTreeModule<T>::isSteinerTree(graph, terminals, isTerminal, *algTree), Equals(true));

		// only check optimum approximation
		// for algorithms with factor of 2 or better
		if(factor != 0 && factor <= 2) {
			AssertThat(algCost, Equals(cost));
			AssertThat(algTree->numberOfNodes(), Equals(tree.numberOfNodes()));
			AssertThat(algTree->numberOfEdges(), Equals(tree.numberOfEdges()));

			List<node> nodes;
			tree.allNodes(nodes);
			for(node v : nodes) {
				AssertThat(algTree->copy(tree.original(v)), !IsNull());
			}

			List<edge> edges;
			tree.allEdges(edges);
			for(edge e : edges) {
				AssertThat(algTree->copy(tree.original(e)), !IsNull());
			}
		}
		delete algTree;
	});
}

/**
 * Tests one subclass of MinSteinerTreeModule for a specific type.
 *
 * \param moduleName
 *	a human readable name/description of the module
 * \alg
 *	the Steiner tree module to be tested
 * \maxNodes
 *	the maximum size of the randomly generated instances for this algorithm
 * \factor
 *	the approximation factor of this algorithm, needed for validating the results
 */
template<typename T>
void testModule(const std::string &moduleName, MinSteinerTreeModule<T> &alg, int maxNodes, double factor)
{
	describe(moduleName, [&]() {
		// only test a subset of the numbers <= maxNodes
		for (int n = 2, nold = 1, tmp; n <= maxNodes; tmp = n, n += nold / 2, nold = tmp) {
				testModuleOnRandomGraph(alg, n, factor);
		}

		for_each_file("steiner", [&](const string &filename){
			// optimal solution value is extracted from the filename
			string tmp = filename.substr(0, filename.length() - 4);
			tmp = tmp.substr(tmp.find_last_of('.') + 1);
			std::stringstream ss(tmp);
			T opt = -1;
			ss >> opt;

			it(string("yields correct results on " + filename + " (optimum is " + to_string(opt) + ")"), [&](){
				EdgeWeightedGraph<T> graph;
				List<node> terminals;
				NodeArray<bool> isTerminal(graph);

				GraphIO::readSTP(graph, terminals, isTerminal, filename);

				EdgeWeightedGraphCopy<T> *algTree;
				T algCost = alg.call(graph, terminals, isTerminal, algTree);

				AssertThat(MinSteinerTreeModule<T>::isSteinerTree(graph, terminals, isTerminal, *algTree), Equals(true));
				delete algTree;
				AssertThat(algCost, IsGreaterThan(opt) || Equals(opt));
				if(factor != 0) {
					AssertThat(algCost, IsLessThan(factor*opt) || Equals(factor*opt));
				}
			});
		});
	});
}

/**
 * Registers one instance of the MinSteinerTreeDirectedCut class for each of its variants
 */
template <typename T>
static void
registerDirectedCutVariants(std::vector<ModuleTuple<T>*> &modules)
{
	for(int i = 0; i <
	  2 * // maximum flow modules
	  2 * // back cuts on/off
	  2 * // min cardinality cuts on/off
	  2 * // nested cuts on/off
	  2;   // all non-dynamic constraints on/off
	  i++) {
		MinSteinerTreeDirectedCut<T> *alg = new MinSteinerTreeDirectedCut<T>();

		std::stringstream ss;
		ss << "DirectedCut";
		int n = 2;

		if(i % n) {
			alg->setMaxFlowModule(new MaxFlowEdmondsKarp<double>());
			ss << ", Edmonds-Karp";
		} else {
			alg->setMaxFlowModule(new MaxFlowGoldbergTarjan<double>());
			ss << ", Goldberg-Tarjan";
		}
		n *= 2;

		alg->useBackCuts(i % n);
		if(i % n) { ss << ", back cuts"; }
		n *= 2;

		alg->useMinCardinalityCuts(i % n);
		if(i % n) { ss << ", min cardinality cuts"; }
		n *= 2;

		alg->useNestedCuts(i % n);
		if(i % n) { ss << ", nested cuts"; }
		n *= 2;

		alg->useDegreeConstraints(i % n);
		alg->useFlowBalanceConstraints(i % n);
		alg->useGSEC2Constraints(i % n);
		alg->useIndegreeEdgeConstraints(i % n);
		ss << (i % n ? ", all constraints enabled" : ", static constraints disabled");

		modules.push_back(new ModuleTuple<T>(ss.str(), alg, 30, 1));
	}
}

/**
 * Registers one instance of the MinSteinerTreeZelikovsky class for each of its variants
 */
template <typename T>
static void
registerZelikovskyVariants(std::vector<ModuleTuple<T>*> &modules)
{
	using WCalc = std::tuple<std::string, typename MinSteinerTreeZelikovsky<T>::WinCalculation>;
	using TGen = std::tuple<std::string, typename MinSteinerTreeZelikovsky<T>::TripleGeneration>;
	using TRed = std::tuple<std::string, typename MinSteinerTreeZelikovsky<T>::TripleReduction>;
	using SCalc = std::tuple<std::string, typename MinSteinerTreeZelikovsky<T>::SaveCalculation>;
	using Pass = std::tuple<std::string, typename MinSteinerTreeZelikovsky<T>::Pass>;
	using APSPStrategy = std::tuple<std::string, bool>;

	std::vector<WCalc> winCalculations = {
		WCalc("absolute win function", MinSteinerTreeZelikovsky<T>::absolute),
		WCalc("relative win function", MinSteinerTreeZelikovsky<T>::relative)
	};
	std::vector<TGen> tripleGenStrategies = {
		TGen("exhaustive triple generation", MinSteinerTreeZelikovsky<T>::exhaustive),
		TGen("Voronoi triple generation", MinSteinerTreeZelikovsky<T>::voronoi),
		TGen("direct triple generation", MinSteinerTreeZelikovsky<T>::ondemand)
	};
	std::vector<TRed> tripleReductStrategies = {
		TRed("enabled reduction", MinSteinerTreeZelikovsky<T>::on),
		TRed("disabled reduction", MinSteinerTreeZelikovsky<T>::off),
	};
	std::vector<SCalc> saveCalculations = {
		SCalc("static enumeration save calculation", MinSteinerTreeZelikovsky<T>::staticEnum),
		SCalc("static LCATree save calculation", MinSteinerTreeZelikovsky<T>::staticLCATree),
		SCalc("dynamic LCATree save calculation", MinSteinerTreeZelikovsky<T>::dynamicLCATree),
		SCalc("hybrid save calculation", MinSteinerTreeZelikovsky<T>::hybrid)
	};
	std::vector<Pass> passes = {
		Pass("one-pass", MinSteinerTreeZelikovsky<T>::one),
		Pass("multi-pass", MinSteinerTreeZelikovsky<T>::multi)
	};
	std::vector<APSPStrategy> apspStrategies = {
		APSPStrategy("forced APSP", true),
		APSPStrategy("SSSP", false),
	};

	std::vector<typename decltype(winCalculations)::size_type>
	  choice = { 0, 0, 0, 0, 0, 0 },
	  maxchoice = {
	    winCalculations.size(),
	    tripleGenStrategies.size(),
	    tripleReductStrategies.size(),
	    saveCalculations.size(),
	    passes.size(),
	    apspStrategies.size(),
	  };
	enum indices {
		winIdx = 0,
		tgenIdx,
		tredIdx,
		saveIdx,
		passIdx,
		apspIdx,
	};

	auto nextChoice = [&]() {
		bool overflow;
		unsigned int i = 0;
		do {
			overflow = false;
			++choice[i];
			choice[i] %= maxchoice[i];
			if (choice[i] == 0) {
				++i;
				overflow = true;
			}
		} while (overflow && i < choice.size());
		return !overflow;
	};

	do {
		string desc = "Zelikovsky: ";

		MinSteinerTreeZelikovsky<T> *module = new MinSteinerTreeZelikovsky<T>();

		Pass pass = passes[choice[passIdx]];
		desc += std::get<0>(pass);
		module->pass(std::get<1>(pass));

		SCalc saveCalc = saveCalculations[choice[saveIdx]];
		desc += ", " + std::get<0>(saveCalc);
		module->saveCalculation(std::get<1>(saveCalc));

		TGen tripleGen = tripleGenStrategies[choice[tgenIdx]];
		desc += ", " + std::get<0>(tripleGen);
		module->tripleGeneration(std::get<1>(tripleGen));

		TRed tripleRed = tripleReductStrategies[choice[tredIdx]];
		desc += ", " + std::get<0>(tripleRed);
		module->tripleReduction(std::get<1>(tripleRed));

		WCalc winCalc = winCalculations[choice[winIdx]];
		desc += ", " + std::get<0>(winCalc);
		module->winCalculation(std::get<1>(winCalc));

		APSPStrategy apspStrategy = apspStrategies[choice[apspIdx]];
		desc += ", " + std::get<0>(apspStrategy);
		module->forceAPSP(std::get<1>(apspStrategy));

		// check for invalid configurations
		if (module->tripleGeneration() == MinSteinerTreeZelikovsky<T>::ondemand
		 && ((module->winCalculation() != MinSteinerTreeZelikovsky<T>::absolute)
		  || (module->saveCalculation() == MinSteinerTreeZelikovsky<T>::hybrid)
		  || (module->tripleReduction() == MinSteinerTreeZelikovsky<T>::off)
		  || (module->pass() == MinSteinerTreeZelikovsky<T>::one))) {
			delete module;
		} else {
			modules.push_back(new ModuleTuple<T>(desc, module, 200, 11/6));
		};
	} while (nextChoice());
}

/**
 * Registers one instance of the MinSteinerTreeRZLoss class for each of its variants
 */
template <typename T>
static void
registerRZLossVariants(std::vector<ModuleTuple<T>*> &modules)
{
	// RZLoss for different maximum component sizes
	for(int i = 2; i < 6; i++) {
		MinSteinerTreeRZLoss<T> *alg = new MinSteinerTreeRZLoss<T>();
		int maxCompSize = i;
		std::string info = "";
		// APSP is only being used for maximum component size of 3
		if(i == 2) {
			alg->forceAPSP(true);
			info = " and forced APSP";
			maxCompSize = 3;
		}
		alg->setMaxComponentSize(maxCompSize);
		modules.push_back(new ModuleTuple<T>("RZLoss with maximum component size of " + to_string(maxCompSize) + info, alg, 100, 2));
	}
}

/**
 * Registers one instance of the MinSteinerTreeGoemans139 class for each of its variants
 */
template <typename T>
static void
registerGoemans139Variants(std::vector<ModuleTuple<T>*> &modules)
{
	// Goemans139 for different maximum component sizes
	for(int i = 2; i < 6; i++) {
		// and for standard and stronger LP relaxation
		for (int strongerLP = 0; strongerLP < 2; ++strongerLP) {
			for (int use2approx = 0; use2approx < 2; ++use2approx) {
				MinSteinerTreeGoemans139<T> *alg = new MinSteinerTreeGoemans139<T>();
				int maxCompSize = i;
				std::string info = "Goemans139 with maximum component size ";
				if(i == 2) {
					alg->forceAPSP();
					maxCompSize = 3;
					info += "3 (enforced APSP)";
				} else {
					info += to_string(maxCompSize);
				}
				alg->setMaxComponentSize(maxCompSize);
				if (strongerLP) {
					alg->separateCycles();
					info += " using stronger LP";
				}
				if (use2approx) {
					alg->use2Approximation();
					info += " with upper bound";
				}
				modules.push_back(new ModuleTuple<T>(info, alg, 100/maxCompSize, 2));
			}
		}
	}
}

/**
 * Registers a complete Steiner test suite for a given
 * template parameter, like int or double.
 */
template<typename T>
void registerSuite(const std::string typeName)
{
	auto typeString = [&typeName](std::string str) {
		return str + string(" [") + typeName + string("]");
	};

	describe(typeString("MinSteinerTreeModule"), [&]() {
		describe(typeString("isSteinerTree"), []() {
			testIsSteinerTree<T>();
		});
	});

	std::vector<ModuleTuple<T>*> modules = {
		new ModuleTuple<T>("DirectedCut default", new MinSteinerTreeDirectedCut<T>(), 75, 1),
		new ModuleTuple<T>("Kou", new MinSteinerTreeKou<T>(), 200, 2),
		new ModuleTuple<T>("Mehlhorn", new MinSteinerTreeMehlhorn<T>(), 200, 2),
		new ModuleTuple<T>("RZLoss default", new MinSteinerTreeRZLoss<T>(), 200, 2),
		new ModuleTuple<T>("Goemans139 default", new MinSteinerTreeGoemans139<T>(), 100, 2),
		new ModuleTuple<T>("Takahashi", new MinSteinerTreeTakahashi<T>(), 200, 2),
		new ModuleTuple<T>("Shore", new MinSteinerTreeShore<T>(), 75, 1),
		new ModuleTuple<T>("Primal-Dual", new MinSteinerTreePrimalDual<T>(), 100, 2),
		new ModuleTuple<T>("DualAscent", new MinSteinerTreeDualAscent<T>(), 150, 0),
		new ModuleTuple<T>("Zelikovsky default", new MinSteinerTreeZelikovsky<T>(), 200, 11/6.0),
	};

	registerDirectedCutVariants<T>(modules);
	registerZelikovskyVariants<T>(modules);
	registerRZLossVariants<T>(modules);
	registerGoemans139Variants<T>(modules);

	// register suites
	for(auto module : modules) {
		testModule<T>(typeString(std::get<0>(*module)), *std::get<1>(*module), std::get<2>(*module), std::get<3>(*module));
		delete std::get<1>(*module);
		delete module;
	}
}

go_bandit([](){
	describe("MinSteinerTreeModule", []() {
		registerSuite<int>("int");
		registerSuite<double>("double");
	});
});


#
#	tradf.py
#
#       - This script implements "Timed Extension of Reactive and Adaptive Dataflow Graph (TRADF)" 
#		that is used to model distributed streaming applications
#
#	- It provides methods that can be used by the optimization engine to derive optimal schedule
#
#	Author: Kamyar Mirzazad (kammirzazad@utexas.edu)
#
#	Abbreviations/Acronyms:
#
#	- "ch"	   : channel
#	- "DP"	   : delivery probability
#	- "DDP"	   : derivative of delivery probability
#	- "NDD"	   : Network Delay Distribution (probabilistic distribution model for one-way ping delays)
#	- "Dprime" : latency budget of the network link (refer to paper)
#

import	math
import	random
import	operator
import	numpy as np
import	networkx as nx
import	scipy.stats as st

from 	scipy import optimize
from    networkx.readwrite import json_graph

def	isclose(a, b, rel_tol=1e-09, abs_tol=0.0):
	return abs(a-b) <= max(rel_tol * max(abs(a), abs(b)), abs_tol)

def	greaterThanEqual(a, b):
	return isclose(a,b) or (a>b)


class	TRADF:

	##
	##	generic methods
	##

	#	includes self-loops
	def	chCount(self):
		return len(self.__graph.edges())

	def	actorCount(self):
		return len(self.__graph)

	def	cycleCount(self):
		return len(self.__cycles)

	def	simplePathCount(self, source, sink):
		return len(self.__simplePaths[(source,sink)])

	def	getSinks(self):
		return self.__sinks

	def	getSources(self):
		return self.__sources

	def	getActors(self):
		return sorted(self.__graph.nodes.keys())

	def	getWCET(self, actor):
		return self.__graph.nodes[actor]['wcet']

	def	getInitialNodes(self):
		return self.__initialNodes

	##
	##	baseline schedule methods
	##

	def	getCycleCh(self, cycleIdx):
		return self.__convertPath2Edges(self.__cycles[cycleIdx])

	def	getSimplePathCh(self, source, sink, pathIdx):
		return self.__convertPath2Edges(self.__simplePaths[(source,sink)][pathIdx])

	#	sum of WCET of actors associated with the given cycle
	def	getCycleWCET(self, cycleIdx):
		raise 'This method is deprecated'
		cycleWCET = 0.0
		for actor in self.__cycles[cycleIdx]:
			cycleWCET += self.__graph.nodes[actor]['wcet']
		return cycleWCET

	#	sum of WCET of actors associated with the given path
	def	getSimplePathWCET(self, source, sink, pathIdx):
		pathWCET = 0.0
		for actor in self.__simplePaths[(source,sink)][pathIdx]:
			pathWCET += self.__graph.nodes[actor]['wcet']
		return pathWCET

	##
	##	optimization constraints/bounds methods
	##

	#	start times' constraints / producer-consumer relationships
	def	getAllPairs(self):
		forwardPairs = []
		backwardPairs = [] # note that backward pairs do not include self-loops

		for edge in self.__graph.edges():

			if self.__isSelfLoop(edge):
				continue

			if not edge in self.__backedges:
				forwardPairs.append(edge)
			else:
				backwardPairs.append(edge) 

		return {'forward':forwardPairs, 'backward':backwardPairs}



	#	"rel":	     used to compare/update start time	=> [if rel(currTS,newTS): currTS = newTS]
	#	"latBudget": callable/dictionary		=> [latBudget(edge) or latBudget[edge]]
	def	scheduleGraph(self, rel, latBudget):
		currStartTimes = self.__scheduleForwardEdges(rel,latBudget)
		nextStartTimes = self.__scheduleBackwardEdges(rel,latBudget,currStartTimes)
		return {'currentStartTimes':currStartTimes, 'nextStartTimes':nextStartTimes}


	#	used to derive absolute latency constraints from 'rho'
	#	Note that if graph is acyclic, reference period constraint won't change with rho
	#	FIXME Think about this! Might be making same mistake as DAC19 paper
	def	getReferenceConstraints(self, percentile):
		# note that "lt" operator guarantees given "percentile" on all channels
		res = self.scheduleGraph(operator.lt, lambda edge: self.__getChLat(edge,percentile))
		return\
		{\
			'period':self.getPeriod(res['currentStartTimes'],res['nextStartTimes']),\
			'latencies':self.getLatencies(res['currentStartTimes'])\
		}


	#	latency constraints has nothing to do with paths/cycles
	def	getLatencies(self, startTimes):
		lats = {}
		for sink in self.__sinks:
			if isclose(startTimes[sink],0):
				print 'WARNING: sink ('+sink+') start time is set to zero, make sure this is valid!'
			endTime = startTimes[sink] + self.__graph.nodes[sink]['wcet']
			for source in self.__sources:
				lats[(source,sink)] = endTime - startTimes[source]
		return lats


	def	getPeriod(self, currStartTimes, nextStartTimes):
		period = 0.0
		for actor in self.__graph.nodes:
			diff = nextStartTimes[actor] - currStartTimes[actor]
			assert diff > 0
			if diff > period:
				period = diff
		return period


	def	verifyConstraints(self, startTimes, pConstr, latConstrs=None):

		#for actor in startTimes:
		#	print actor, startTimes[actor]

		for actor in startTimes:
			assert greaterThanEqual(startTimes[actor],0.0)


		# latency constraints
		if latConstrs != None:
			for pair in latConstrs:
				lat = (startTimes[pair[1]] + self.getWCET(pair[1])) - startTimes[pair[0]]
				assert greaterThanEqual(latConstrs[pair],lat)


		for edge in self.__graph.edges():

			if self.__isSelfLoop(edge):
				continue

			u, v = edge

			if edge in self.__backedges:			
				# source of backedge finishes before its destination fires for second time
				assert greaterThanEqual(startTimes[v] + pConstr, startTimes[u] + self.getWCET(u))
			else:
				#print u,v, startTimes[u], startTimes[v]
				assert greaterThanEqual(startTimes[v], startTimes[u] + self.getWCET(u))


	##
	##	optimization goal methods
	##

	def	getStatsLinear(self, startTimes, period):
		backedgeDeltas = {} # used to propogate quality from one iteration to next one
		actors = self.getActors()

		if not self.__isCyclic:
			self.__initQvars(actors,backedgeDeltas)
			self.__propagateQvars(actors,startTimes,period)

		else:
			for i in range(5):
				#print '@',i,'th quality iteration'
				self.__initQvars(actors,backedgeDeltas)
				self.__propagateQvars(actors,startTimes,period)

				for edge in self.__backedges:
					if not self.__isSelfLoop(edge):
						backedgeDeltas[edge[1]] = self.__getDeltas(actors,startTimes,period,edge)
		
		return	self.__calculateMetric()


	#	calculate delivery probability and its derivative for all channels
	def	setAllChStat(self, startTimes, period):
		for edge in self.__graph.edges(): # includes forward/backward edges and self-loops
			self.__setChStat(edge,startTimes,period)


	def	getScheduledGraph(self, startTimes, period, estSNR):
		self.setAllChStat(startTimes,period)
		self.__setChAttrs(startTimes,period)
		self.__setActorAttrs(startTimes)
		data = json_graph.node_link_data(self.__graph,{'name':'name','link':'channels'})

		##	add period 
		data['period'] = self.__conv2str(period)

		##	add estimated SNR
		data['estimatedSNR'] = str(estSNR)+'dB'

		##	add execution order
		data['executionOrder'] = self.__executionOrder

		##
		##	add source/sink latencies
		##

		serializedLats = {} 
		lats = self.getLatencies(startTimes)
		for pair in lats:
			serializedLats['('+pair[0]+','+pair[1]+')'] = self.__conv2str(lats[pair])
		data['latencies'] = serializedLats


		##
		##	touch-up actors
		##

		data['actors'] = data['nodes']
		for actor in data['actors']:
			actor.pop('sigVal',None)
			actor.pop('pNoisy',None)
			actor.pop('gradient',None)
			actor['ts'] = self.__conv2str(actor['ts'])
			actor['wcet'] = self.__conv2str(actor['wcet'])

		##
		##	touch-up channels
		##

		channels = []
		# add output channels
		for actor in self.__sinks:
			channels.append({'source':actor, 'weight':self.__sinks[actor]})
		# add input channels
		for actor in self.__sources:
			channels.append({'source':self.__sources[actor]['input'], 'weight':self.__sources[actor]['weight'], 'target':actor})
		# remove self-loops, mark backedges, ...
		for ch in data['channels']:
			edge = (ch['source'],ch['target'])

			if self.__isSelfLoop(edge):
				continue

			ch2 = ch.copy()
			ch2.pop('ndd',None)
			ch2.pop('ddp',None)

			if edge in self.__backedges:
				ch2['hasInitialToken'] = True

			channels.append(ch2)

		data['channels'] = channels


		##
		##	remove unnecessary data
		##
	
		data.pop('graph',None)
		data.pop('nodes',None)
		data.pop('directed',None)
		data.pop('multigraph',None)

		return data


	##
	##	helper methods for calculating quality
	##

	def	__initQvars(self, actors, backedgeDeltas):
		actorCnt = len(actors)
		# note that actor might be both a source of graph and destination of a backedge
		for actor in actors:
			# initialize all fields of actor
			self.__graph.nodes[actor]['sigVal'] = 0.0
			self.__graph.nodes[actor]['pNoisy'] = 0.0
			self.__graph.nodes[actor]['gradient'] = np.full(actorCnt,0.0)

			if actor in self.__sources:
				w = abs(self.__sources[actor]['weight'])
				self.__graph.nodes[actor]['sigVal'] += w
				self.__graph.nodes[actor]['pNoisy'] += pow(w,2)

			if actor in backedgeDeltas:
				self.__graph.nodes[actor]['sigVal']   += backedgeDeltas[actor]['sigVal']
				self.__graph.nodes[actor]['pNoisy']   += backedgeDeltas[actor]['pNoisy']
				self.__graph.nodes[actor]['gradient'] += backedgeDeltas[actor]['gradient']


	def	__propagateQvars(self, actors, startTimes, period):
		for order in self.__schedulingOrders:
			for edge in order:
				v = edge[1]
				deltas = self.__getDeltas(actors,startTimes,period,edge)
				self.__graph.nodes[v]['sigVal']   += deltas['sigVal']
				self.__graph.nodes[v]['pNoisy']	  += deltas['pNoisy']
				self.__graph.nodes[v]['gradient'] += deltas['gradient']


	def	__getDeltas(self, actors, startTimes, period, edge):
		u, v = edge
		uIdx = actors.index(u)
		vIdx = actors.index(v)

		# calculate dprime
		srcEndTime = startTimes[uIdx] + self.__graph.nodes[u]['wcet']
		dstStartTime = startTimes[vIdx]
		if edge in self.__backedges:
			dstStartTime += period
		dprime = dstStartTime-srcEndTime
		#assert dprime >= 0.0

		# derive weights and probabilities
		ndd = self.__getNDD(edge)
		w = abs(self.__graph[u][v]['weight'])
		cdf = (1-ndd['u']) * self.__evalNDD('cdf', dprime, ndd)
		pdf = (1-ndd['u']) * self.__evalNDD('pdf', dprime, ndd)
				
		pNoisy = self.__graph.nodes[u]['pNoisy']
		gradient = self.__graph.nodes[u]['gradient']

		# calculate partial gradient update
		if not self.__isCyclic:
			assert gradient[vIdx] == 0.0	# gradient w.r.t actor's start time should not have been initialized before

		gradUpdate = cdf * gradient
		gradUpdate[uIdx] -= pdf * pNoisy	# another term that contributes to derivative w.r.t. producer's start time
		gradUpdate[vIdx] += pdf * pNoisy	# initialize derivative w.r.t. actor's start time

		return\
		{\
			'sigVal': w * self.__graph.nodes[u]['sigVal'],\
			'pNoisy': pow(w,2) * cdf * pNoisy,\
			'gradient': pow(w,2) * gradUpdate\
		}


	def	__calculateMetric(self):
		totalWeight = 0.0
		weightedSNR = 0.0
		weightedGradient = np.full(self.actorCount(),0.0)

		for sink in self.__sinks:			
			w = self.__sinks[sink] # note that we don't apply ABS to sink's weight
			pSignal = pow(self.__graph.nodes[sink]['sigVal'],2)
			pNormNoise = self.__graph.nodes[sink]['pNoisy']/pSignal

			#print sink, 1.0/(1.0-pNormNoise), 1e+4*pSignal, 1e+4*(pSignal-self.__graph.nodes[sink]['pNoisy'])

			totalWeight += w
			weightedSNR += w * (1.0/(1.0-pNormNoise))
			weightedGradient += w * (self.__graph.nodes[sink]['gradient']/(pSignal * pow(1.0-pNormNoise,2)))

		return -1.0*(weightedSNR/totalWeight), -1.0*(weightedGradient/totalWeight)



	def	__setChStat(self, edge, startTimes, period):
		u, v = edge
		if self.__isSelfLoop(edge):
			self.__graph[u][v]['dp'] = 1.0
			self.__graph[u][v]['ddp'] = 0.0
			return

		ndd = self.__getNDD(edge)
		dprime = self.__getDprime(edge,startTimes,period)
		self.__graph[u][v]['dp'] = (1-ndd['u']) * self.__evalNDD('cdf', dprime, ndd)
		self.__graph[u][v]['ddp'] = (1-ndd['u']) * self.__evalNDD('pdf', dprime, ndd)


	def	__getDprime(self, edge, startTimes, period):
		assert not self.__isSelfLoop(edge)
		srcEndTime = startTimes[edge[0]] + self.__graph.nodes[edge[0]]['wcet']
		dstStartTime = startTimes[edge[1]]

		if edge in self.__backedges:
			dstStartTime += period

		#if dstStartTime < srcEndTime:
		#	self.__printWarning(edge,startTimes,period)

		return (dstStartTime-srcEndTime)


	def	__printWarning(self, edge, startTimes, period):
		t0 = str(round(startTimes[edge[0]],3))
		t1 = str(round(startTimes[edge[1]],3))
		wcet = str(round(self.__graph.nodes[edge[0]]['wcet'],3))
		print 'WARNING: dprime was negative [Ts('+edge[0]+')='+t0+', WCET('+edge[0]+')='+wcet+', Ts('+edge[1]+')='+t1+', period='+str(period)+']'

	##
	##	helper methods for scheduling the graph
	##

	def	__getChLat(self, edge, percentile):
		assert not self.__isSelfLoop(edge)
		# in case pdf of NDD is non-zero for lat<0, make sure "chLat" is positive 
		return max(0.0,self.__evalNDD('ppf', percentile, self.__getNDD(edge)))


	def	__scheduleForwardEdges(self, rel, latBudget):
		# initialize start times
		initialTS = 0.0 if rel(0,1) else np.inf
		startTimes = dict.fromkeys(self.__graph.nodes.keys(),initialTS)
		for node in self.__initialNodes:
			startTimes[node] = 0.0

		# schedule one component of precedence graph at a time
		for order in self.__schedulingOrders:
			for edge in order:
				u, v = edge
				newTS = startTimes[u] + self.__graph.nodes[u]['wcet'] + (latBudget(edge) if callable(latBudget) else latBudget[edge])
				if rel(startTimes[v],newTS):
					startTimes[v] = newTS
		return startTimes


	def	__scheduleBackwardEdges(self, rel, latBudget, currStartTimes):
		nextStartTimes = currStartTimes.copy()
		for edge in self.__backedges:
			u, v = edge

			newTS = currStartTimes[u] + self.__graph.nodes[u]['wcet']
			if not self.__isSelfLoop(edge):
				newTS += latBudget(edge) if callable(latBudget) else latBudget[edge]

			if rel(nextStartTimes[v],newTS):
				nextStartTimes[v] = newTS
			
		return nextStartTimes


	##
	##	functions used to generate final graph
	##

	def	__setActorAttrs(self, startTimes):
		for actor in self.__graph.nodes:
			assert startTimes[actor] >= 0
			self.__graph.nodes[actor]['ts'] = startTimes[actor]


	def	__setChAttrs(self, startTimes, period):
		for edge in self.__graph.edges():
			if self.__isSelfLoop(edge):
				continue
			u, v = edge
			self.__graph[u][v]['dp'] = round(self.__graph[u][v]['dp'],3) # make it printable
			self.__graph[u][v]['mem'] = self.__calcBuffersize(edge,startTimes,period)
			self.__graph[u][v]['dprime'] = self.__conv2str(self.__getDprime(edge,startTimes,period))
			assert self.__graph[u][v]['dprime'] > 0


	def	__calcBuffersize(self, edge, startTimes, period):
		if self.__isSelfLoop(edge):
			return 0
		epsilon = 1e-3
		dprime = self.__getDprime(edge,startTimes,period)
		minNDD = self.__evalNDD('ppf', epsilon,   self.__getNDD(edge))
		maxNDD = self.__evalNDD('ppf', 1-epsilon, self.__getNDD(edge))
		return int(math.ceil((dprime+(maxNDD-minNDD))/period))

	##
	##	generic helper functions
	##	

	def	__getNDD(self, edge):
		return self.__graph[edge[0]][edge[1]]['ndd']

	def	__isSelfLoop(self, edge):
		return (edge[0] == edge[1])

	def	__getPathInput(self, sink, pathIdx):
		src = self.__getPathSource(sink, pathIdx)
		return self.__sources[src]['input']

	def	__getPathSource(self, sink, pathIdx):
		# source actor of the first edge of the path
		return self.__pathEdges[sink][pathIdx][0][0]

	def	__evalNDD(self, funcName, x, ndd):
		dist = getattr(st, ndd['dist'])
		func = getattr(dist, funcName)
		
		if 'shape' in ndd:
			return func(x, ndd['shape'], loc=ndd['loc'], scale=ndd['scale'])
		else:
			return func(x, loc=ndd['loc'], scale=ndd['scale'])

	##
	##	initialization functions
	##

	def	__init__(self, graphDesc, netDesc):

		# initialize variables
		self.__isCyclic = False
		self.__sinks = {}			# outputs of TRADF graph
		self.__cycles = []			# set of paths that extend from an initial node of a component to an initial node of another component
		self.__sources = {}			# inputs of TRADF graph
		self.__backedges = []			# edges with initial token		
		self.__simplePaths = {}			# dictionary of simple paths in edge-format, indexed by (source,sink)
		self.__initialNodes = []		# set of nodes with no producer in any component of the precedence graph
		self.__graph = nx.DiGraph()		# TRADF graph with NDD attached to channels
		self.__executionOrder = []		# order of actor execution in centralized case (used for simulation)
		self.__schedulingOrders = []		# the order actors of each component should be scheduled
		self.__precedenceGraph = nx.DiGraph()	# input graph without backedges

		# set variables
		self.__loadGraph(graphDesc,netDesc)
		self.__verifyExternal()
		self.__analyzeComponents()

		for sink in self.__sinks:
			for source in self.__sources:
				pair = (source,sink)
				self.__simplePaths[pair] = []
				for path in nx.all_simple_paths(self.__precedenceGraph, source=source, target=sink):
					self.__simplePaths[pair].append(path)
					#print 'simple path from', source, 'to', sink, ':', path


	def	__loadGraph(self, graphDesc, netDesc):
		for actor in graphDesc['actors']:
			wcet = self.__conv2ms(actor['wcet'])
			self.__graph.add_node(actor['name'], host=actor['host'], wcet=wcet, ts=0.0, sigVal=0.0, pNoisy=0.0, gradient=None)
			self.__precedenceGraph.add_node(actor['name'])

		for ch in graphDesc['channels']:

			if not 'target' in ch:
				name = ch['source']				
				self.__sinks[name] = ch['weight']
				#print 'actor "'+name+'"\'s output SNR has weight of', ch['weight']
				continue

			src = ch['source']
			dst = ch['target']

			if not src in self.__graph.nodes:
				self.__sources[dst] = {'input':src, 'weight':ch['weight']}
				#print 'actor "'+name+'" is connected to input "'+ch['source']+'" with weight of', ch['weight']
				continue

			if 'hasInitialToken' in ch:
				self.__backedges.append((src,dst))
				self.__isCyclic |= (src != dst)
			else:
				self.__precedenceGraph.add_edge(src,dst)

			self.__graph.add_edge(src, dst, weight=ch['weight'], ndd=self.__lookupNDD(ch,netDesc), dp=0, ddp=0, mem=0)


		print 'Graph is', ('cyclic,' if self.__isCyclic else 'acyclic,'), 'has', self.actorCount(), 'actors and', self.chCount(), 'channels.'
		
		# debug only
		if False:
			edges = []
			for edge in self.__backedges:
				if self.__isSelfLoop(edge):
					continue
				edges.append(edge)
			print "Not self-loop backedges:", edges


	def	__analyzeComponents(self):
		counts = []
		# FIXME: Do we need to separate subgraphs with only one actor?
		for subActors in nx.weakly_connected_components(self.__precedenceGraph):
			print subActors
			counts.append(len(subActors))
			subG = self.__precedenceGraph.subgraph(subActors)
			iNodes = self.__getInitialNodes(subG)
			self.__addCycles(subG,iNodes)
			self.__initialNodes += iNodes
			self.__updateOrders(subG)
			#print self.__schedulingOrders[-1]
		print 'Size of components of precedence graph', counts


	def	__updateOrders(self, subGraph):
		partialExeOrder = list(nx.topological_sort(subGraph))
		self.__executionOrder += partialExeOrder

		schedOrder = []
		for actor in partialExeOrder:
			for prod in subGraph.predecessors(actor):
				schedOrder.append((prod,actor))
		self.__schedulingOrders.append(schedOrder)


	def	__addCycles(self, subGraph, iniNodes):
		# check which backedges have their source in this subgraph
		for edge in self.__backedges:

			if self.__isSelfLoop(edge):
				continue

			if edge[0] in subGraph.nodes:
				for iniNode in iniNodes:
					# add all paths from any initial node to source of backedge to cycles
					if iniNode == edge[0]:
						#print iniNode, [edge[0],edge[1]]
						self.__cycles.append([edge[0],edge[1]])
					else:
						for path in nx.all_simple_paths(subGraph, source=iniNode, target=edge[0]):
							path2 = list(path)
							path2.append(edge[1]) # cycle includes destination of backedge too
							#print iniNode, path2
							self.__cycles.append(path2)


	#	finds initial nodes of this subgraph (OR should it be subset of "source" actors?)
	def	__getInitialNodes(self, subGraph):
		iniNodes = []
		for node in subGraph.nodes:
			if not self.__hasProducer(subGraph,node):
				iniNodes.append(node)
		print 'initialNodes are', iniNodes
		return iniNodes


	def	__hasProducer(self, graph, node):
		if graph.in_degree(node) == 0:
			return False		
		if (graph.in_degree(node) == 1) and (list(graph.predecessors(node))[0] == node):
			return False
		return True


	def	__hasConsumer(self, graph, node):
		if graph.out_degree(node) == 0:
			return False
		if (graph.out_degree(node) == 1) and (list(graph.successors(node))[0] == node):
			return False
		return True


	def	__convertPath2Edges(self, path):
		edges = []
		for i in range(len(path)-1):
			edges.append((path[i],path[i+1]))
		return edges


	def	__lookupNDD(self, ch, netDesc):
		# self-loops have no NDD
		if ch['source'] == ch['target']:
			return None

		srcHost = self.__graph.nodes[ch['source']]['host']
		dstHost = self.__graph.nodes[ch['target']]['host']
		return netDesc[srcHost][dstHost]


	def	__verifyExternal(self):
		for node in self.__graph.nodes:

			if not self.__hasProducer(self.__graph,node):

				if not node in self.__sources:
					print 'actor "'+node+'" has no producer and is not connected to any input'
					exit(1)

				if not self.__hasConsumer(self.__graph,node):
					print 'actor "'+node+'" is isolated from rest of the graph'
					exit(1)

			if not self.__hasConsumer(self.__graph,node):
				if not node in self.__sinks:
					print 'actor "'+node+'" has no consumer and its output is not weighted'
					exit(1)
		# DEBUG-only
		#print 'source:'+self.__sources, 'sink:'+self.__sinks


	##
	##	utility functions
	##

	def	__conv2str(self, val):
		return str(round(val,3))+'ms'

	def	__conv2ms(self, val):
		numeric = '0123456789.'
		for i,c in enumerate(val):
			if c not in numeric:
				break

		number = float(val[:i])
		unit = val[i:]

		if unit == 'us':
			return (number/1000.0)
		elif unit == 'ms':
			return number
		elif unit == 's':
			return (1000.0*number)
		else:
			print 'unknown unit', unit
			exit(1)


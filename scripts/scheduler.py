
import	operator
import	numpy as np

from	tradf		import	TRADF
from	random		import	uniform
from	scipy.optimize	import	Bounds
from	scipy.optimize	import	minimize
from	scipy.optimize	import	LinearConstraint


class	Scheduler:

	def	optimize_by_rho(self, rho):
		constrs = self.__getConstraints(rho)

		# for debugging
		if False:
			# make latency constraints printable
			latConstrs = {}
			for pair in constrs['latencyConstraints']:
				latConstrs[(str(pair[0]),str(pair[1]))] = round(constrs['latencyConstraints'][pair],3)
			print 'period constraint:', round(constrs['periodConstraint'],3)
			print 'latency constraints:', latConstrs
		
		return self.optimize(constrs['periodConstraint'], constrs['latencyConstraints'])
		

	def	optimize(self, pConstr, latConstrs):
		lc = self.__getLinearConstraints(pConstr,latConstrs)
		minGoal = lambda startTimes: self.__graph.getStatsLinear(startTimes,pConstr)
		startTimes0 = self.__generateSchedule(pConstr,latConstrs,self.__allocateBudgetUniform)

		x0 = self.__convert2Array(startTimes0)

		opt = {}
		opt['xtol'] = 1e-18#1e-4
		opt['gtol'] = 1e-18#1e-5
		#opt['verbose'] = 1
		#opt['maxiter'] = 1e+6
		opt['initial_tr_radius'] = 0.5
		#opt['initial_constr_penalty'] = 10.0
		
		# could also use 'SLSQP' method
		# jac=True
		
		res = minimize(minGoal, x0, jac=True, bounds=self.__getBounds(), constraints=lc, method='trust-constr', options=opt)

		if not res.success:
			print 'optimization failed ('+res.message+')'

		x1 = res.x

		# note that minGoal sets DPs
		y0 = minGoal(x0)[0]
		y1 = minGoal(x1)[0]

		# for debugging
		if False:
			print 'x0:', x0
			print 'x1:', x1

		# for debugging
		if False:
			print 'y0:', y0
			print 'y1:', y1
			print '%y:', (100*(y1-y0))/abs(y0)

		# goal to optimization was to make sure y1 is less than y0
		if y1 > y0:
			print 'optimization failed to find a better solution (',round(y0,3),'<',round(y1,3),')'
			x1 = x0
			y1 = y0	

		startTimes1 = self.__convert2Dict(x1)
		self.__graph.verifyConstraints(startTimes1,pConstr,latConstrs)

		snr0_dB = 10*np.log10(-y0)
		snr1_dB = 10*np.log10(-y1)

		return\
		{\
			'x0':x0,\
			'x1':x1,\
			'y0':y0,\
			'y1':y1,\
			'estSNR0':snr0_dB,\
			'estSNR1':snr1_dB,\
			'graph_stat':self.__graphStats,\
			'execution_time':res.execution_time,\
			'baseline_graph':self.__graph.getScheduledGraph(startTimes0,pConstr,snr0_dB),\
			'optimized_graph':self.__graph.getScheduledGraph(startTimes1,pConstr,snr1_dB)
		}


	def	random(self, rho):
		constr = self.__getConstraints(rho)
		pConstr = constr['periodConstraint']
		latConstrs = constr['latencyConstraints']

		startTimes = self.__generateSchedule(pConstr,latConstrs,self.__allocateBudgetRandom)

		x0 = self.__convert2Array(startTimes)
		y0 = self.__graph.getStatsLinear(x0,pConstr)[0]
		snr0_dB = 10*np.log10(-y0)

		return\
		{\
			'x0':x0,\
			'y0':y0,\
			'estSNR0':snr0_dB,\
			'graph_stat':self.__graphStats,\
			'baseline_graph':self.__graph.getScheduledGraph(startTimes,pConstr,snr0_dB)
		}
		

	def	manual(self, p, startTimes):
		x0 = self.__convert2Array(startTimes)
		self.__graph.verifyConstraints(startTimes,p)
		y0, grad = self.__graph.getStatsLinear(x0,p)

		return\
		{\
			'x0':x0,\
			'y0':y0,\
			'estSNR0':10*np.log10(-y0),\
			'graph_stat':self.__graphStats,\
			'baseline_graph':self.__graph.getScheduledGraph(startTimes,p)
		}


	##
	##	Helper methods
	##	

	#	generate a schedule that follows "allocFunc" and satisfies pConstr/latConstrs
	def	__generateSchedule(self, pConstr, latConstrs, allocFunc):
		# set dprime of backedges to pConstr
		dprimes = dict.fromkeys(self.__pairs['backward'], pConstr)

		"""
		for cIdx in range(self.__graph.cycleCount()):
			slack = pConstr - self.__graph.getCycleWCET(cIdx)
			print pConstr, self.__graph.getCycleWCET(cIdx)
			assert slack > 0
			self.__setDprimes(dprimes,self.__graph.getCycleCh(cIdx),slack,allocFunc)
		"""

		# assign dprime of forward edges based on latConstrs
		for pair in latConstrs:
			for pathIdx in range(self.__graph.simplePathCount(pair[0],pair[1])):
				slack = latConstrs[pair] - self.__graph.getSimplePathWCET(pair[0],pair[1],pathIdx)
				assert slack > 0
				chArr = self.__graph.getSimplePathCh(pair[0],pair[1],pathIdx)
				self.__setDprimes(dprimes,chArr,slack,allocFunc)

		#for pair in dprimes:
		#	print pair, dprimes[pair]

		res = self.__graph.scheduleGraph(operator.lt,dprimes)

		self.__graph.verifyConstraints(res['currentStartTimes'],pConstr,latConstrs)

		return res['currentStartTimes']


	def	__getLinearConstraints(self, pConstr, latConstrs):
		constrIdx = 0
		numConstrs = len(self.__pairs['forward']) + len(self.__pairs['backward']) + len(latConstrs)

		ub = np.full(numConstrs,np.inf)
		lb = np.full(numConstrs,-np.inf)
		A  = np.zeros(shape=(numConstrs,self.__actorCnt))		

		# end time of consumer should be before its producer's start time
		# Ts[v] >= Te[u]
		# Ts[v] >= Ts[u] + WCET[u]
		# Ts[v] - Ts[u] >= WCET[u]
		for pair in self.__pairs['forward']:
			self.__updateMatrix(A,constrIdx,pair)
			lb[constrIdx] = self.__graph.getWCET(pair[0])
			#print 'Ts('+str(dstIdx)+')-Ts('+str(srcIdx)+') >= '+str(lb[constrIdx])
			constrIdx += 1


		# end time of source of a backedge should be before its consumer's next firing
		# Ts[v] + P >= Te[u] 
		# Ts[v] + P >= Ts[u] + WCET[u]
		# Ts[v] - Ts[u] >= WCET[u] - P
		for pair in self.__pairs['backward']:
			self.__updateMatrix(A,constrIdx,pair)
			lb[constrIdx] = self.__graph.getWCET(pair[0])-pConstr
			#print 'Ts('+str(srcIdx)+')-Ts('+str(dstIdx)+') >= '+str(ub[constrIdx])
			constrIdx += 1


		# literal translation of latency constraints
		# latConstr >= Te[v] - Ts[u]
		# latConstr >= Ts[v] + WCET[v] - Ts[u]
		# latConstr - WCET[v] >= Ts[v] - Ts[u] 
		for pair in latConstrs:
			self.__updateMatrix(A,constrIdx,pair)
			ub[constrIdx] = latConstrs[pair]-self.__graph.getWCET(pair[1])
			constrIdx += 1

		assert constrIdx == numConstrs

		return [LinearConstraint(A,lb,ub,keep_feasible=False)]


	##
	##	latency allocation functions for random/baseline scheduling
	##

	def	__allocateBudgetUniform(self, totalBudget, numElements):
		return np.full(numElements,totalBudget/numElements)

	def	__allocateBudgetRandom(self, totalBudget, numElements):
		shares = np.full(numElements,1.0)
		for i in range(numElements):
			# allow share of each link to be up to 40% more/less than average share
			shares[i] += uniform(-0.4,0.4) 
		return (totalBudget/np.sum(shares)) * shares


	def	__setDprimes(self, dprimes, chArr, totalSlack, allocFunc):
		numCh = len(chArr)
		chSlacks = allocFunc(totalSlack,numCh)

		for i in range(numCh):
			ch = chArr[i]

			if not ch in dprimes:
				dprimes[ch] = chSlacks[i]
				continue

			if dprimes[ch] > chSlacks[i]:
				dprimes[ch] = chSlacks[i]


	##
	##	utility functions
	##

	def	__updateMatrix(self, A, constrIdx, pair):
		u, v = pair
		srcIdx = self.__actor2index[u]
		dstIdx = self.__actor2index[v]
		A[constrIdx][srcIdx] = -1
		A[constrIdx][dstIdx] = +1


	#	derive absolute period/latency constraints from given 'rho'	
	def	__getConstraints(self, rho):
		minConstr = self.__graph.getReferenceConstraints(0.001)
		maxConstr = self.__graph.getReferenceConstraints(0.999)
		print minConstr
		print maxConstr

		latConstrs = {}
		pConstr = self.__scale(minConstr['period'],maxConstr['period'],rho)
		for pair in minConstr['latencies']:
			latConstrs[pair] = self.__scale(minConstr['latencies'][pair],maxConstr['latencies'][pair],rho)

		return {'latencyConstraints':latConstrs, 'periodConstraint':pConstr}


	def	__getBounds(self):
		lb = np.full(self.__actorCnt,0)
		ub = np.full(self.__actorCnt,np.inf)

		# start time of initial nodes should be zero
		#for actor in self.__graph.getInitialNodes():
		#	ub[self.__actor2index[actor]] = 0.0

		return Bounds(lb,ub,keep_feasible=False)


	def	__convert2Dict(self, valArr):
		valDict = {}
		for actorIdx in range(self.__actorCnt):
			valDict[self.__index2actor[actorIdx]] = valArr[actorIdx]
		return valDict

	def	__convert2Array(self, valDict):
		valArr = np.full(self.__actorCnt,0.0)
		for actor in valDict:
			valArr[self.__actor2index[actor]] = valDict[actor]
		return valArr

	def	__scale(self, minVal, maxVal, scale):
		return minVal+scale*(maxVal-minVal)


	##
	##	initialization functions
	##

	def	__init__(self, graphDesc, networkDesc):
		self.__graph = TRADF(graphDesc,networkDesc)
		self.__actorCnt = self.__graph.actorCount()
		self.__pairs = self.__graph.getAllPairs()
		self.__indexActors()
		self.__generateGraphStats()


	def	__indexActors(self):
		self.__actor2index = {}
		self.__index2actor = {}
		actors = self.__graph.getActors()
		for i in range(len(actors)):
			self.__index2actor[i] = actors[i] 
			self.__actor2index[actors[i]] = i


	def	__generateGraphStats(self):
		pathCounts = {}
		for sink in self.__graph.getSinks():
			sPathCnts = {}
			for source in self.__graph.getSources():
				sPathCnts[source] = self.__graph.simplePathCount(source,sink)
			pathCounts[sink] = sPathCnts

		self.__graphStats =\
		{\
			'pathCounts':pathCounts,\
			'actorCount':self.__actorCnt,\
			'cycleCount':self.__graph.cycleCount(),\
			'channelCount':self.__graph.chCount(),\
			'backedgeCount':len(self.__pairs['backward'])
		}

		#print self.__graphStats


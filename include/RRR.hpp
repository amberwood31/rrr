// RRR - Robust Loop Closing over Time
// Copyright (C) 2014 Y.Latif, C.Cadena, J.Neira
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#ifndef RRR_HPP_
#define RRR_HPP_


#include <iostream>
#include <algorithm>
#include <numeric>
#include <chrono>


#include "g2o_Interface.hpp"
#include "cluster.hpp"
#include "utils.hpp"


template <class BackEndInterface> //!< @param BackEndInterace one of the back-end
class RRR
{
	Clusterizer clusterizer;
	BackEndInterface* gWrapper;

	int clusteringThreshold;
	int nIterations;
	int ID_IGNORE;

	int edgeDimension;

	IntSet _goodSet;


public:

	RRR(
		int ClusteringThreshold = 50,
		int numIterations = 4
			):
				clusteringThreshold(ClusteringThreshold),
				nIterations(numIterations)
	{
		ID_IGNORE 			= -2;
		gWrapper 			= new BackEndInterface;
		edgeDimension 		= gWrapper->edgeDimension();
	}

	bool read(const char* filename)
	{
		IntPairSet loops;
		bool status = gWrapper->read(filename);		// Read the g2o file
		if(!status) return status;
		gWrapper->getLoopClosures(loops);
		
		std::cerr<<"Number of loop closures : "<<loops.size()<<std::endl;
		
		clusterizer.clusterize(loops,clusteringThreshold);	// Make clusters
		return true;
	}

	bool setOptimizer(void* optimizerPtr)
	{
		IntPairSet loops;
		gWrapper->setOptimizer(optimizerPtr);
		gWrapper->getLoopClosures(loops);
		
		std::cerr<<"Number of loop closures : "<<loops.size()<<std::endl;

		clusterizer.clusterize(loops,clusteringThreshold);	// Make clusters
		
		return true;
	}

	bool intraClusterConsistent(int clusterID)
	{
		IntPairDoubleMap chi2LinkErrors;
		IntPairSet& currentCluster = clusterizer.getClusterByID(clusterID);
		  
		// ctime() used to give the present time 


		chi2LinkErrors = gWrapper->optimize(currentCluster, nIterations);


		float activeChi2Graph = chi2LinkErrors[IntPair(-1,0)];
		int   activeEdgeCount = chi2LinkErrors[IntPair(-1,-1)];

		//if( activeChi2Graph < utils::chi2(edgeDimension* activeEdgeCount)) // conduct test for every cluster, despite the cluster error
		{
			IntPairDoubleMap::iterator eIt = chi2LinkErrors.begin(), eEnd=chi2LinkErrors.end();
			// chi2LinkErrors.begin() gives the (-1, -1, number_of_edges)
			//IntPairSet::iterator it = currentCluster.begin(), end = currentCluster.end();
            std::cout<<" Cluster "<<clusterID<<" started with "<<currentCluster.size()<<" links "<<std::endl;
			for(; eIt!=eEnd; eIt++)
			{
				if(eIt->second > utils::chi2(edgeDimension))
				{
				    clusterizer.setClusterID(eIt->first,ID_IGNORE);
					if (eIt->first.first != -1) {// chi2LinkErrors.begin() returns the (-1, -1, number_of_edges)
                        std::cout << "REJECTED: " << eIt->first.first << " " << eIt->first.second << std::endl;
                        std::cout << "Edge error: " << eIt->second << " > " << utils::chi2(edgeDimension) << std::endl;
                        std::cout << "Edge dimension: " << edgeDimension << std::endl;

					}

				}
			}

			if(!currentCluster.empty())
			{
				std::cerr<<" Cluster "<<clusterID<<" survived with "<<currentCluster.size()<<" links "<<std::endl;
				return true;
			}
			else
			{
				std::cerr<<" Cluster "<<clusterID<<" has been emptied!"<<std::endl;
				return false;
			}

		}
		//else
		//{
            //std::cerr<<" Cluster "<<clusterID<<" has been eliminated!"<<std::endl;
			//std::cout<<" actual chi2 errror: " << activeChi2Graph << std::endl;
			//std::cout<<"dof: " << edgeDimension* activeEdgeCount << std::endl;
			//std::cout<<"chi2 threshold: " << utils::chi2(edgeDimension* activeEdgeCount) << std::endl;
			//return false;
		//}
		

		return false;

	}

	bool gatherLinks(const IntSet& clusterList, IntPairSet& links)
	{
		IntSet::const_iterator it = clusterList.begin(), end = clusterList.end();

		for( ; it!=end; it++)
		{
			IntPairSet& currentCluster = clusterizer.getClusterByID(*it);
			links.insert(currentCluster.begin(),currentCluster.end());
		}
		return true;
	}

	bool interClusterConsistent(IntSet& H, IntSet& goodSet, IntSet& RejectSet)
	{

		if(H.empty()) return true;

		IntPairSet activeLinks;

		gatherLinks(goodSet,activeLinks);
		gatherLinks(H,activeLinks);

		IntPairDoubleMap
			linkErrors = gWrapper->optimize(activeLinks,nIterations);

		float activeChi2Graph = linkErrors[IntPair(-1,0)];
		int   activeEdgeCount = linkErrors[IntPair(-1,-1)];

		double allLinksError = 0;
		for ( IntPairDoubleMap::iterator it = linkErrors.begin(), end = linkErrors.end();
				it!=end;
				it++)
		{
			if(it->first.first >= 0) // Special case for the two values we returned with a -1 as the first
				allLinksError += it->second;
		}



		if(
				activeChi2Graph 	< 	utils::chi2(edgeDimension*activeEdgeCount)
				and
				allLinksError 		< 	utils::chi2(edgeDimension*activeLinks.size())
			)
		{
			goodSet.insert(H.begin(),H.end());
			return true; // all done .. we found a consistent solution
		}
		else
		{
			// Find which cluster is causing the problems
			// Iterate over everything
			// Find the error for each cluster
			// Sum cluster-wise

			IntPairSet::iterator it = activeLinks.begin(), end = activeLinks.end();

			std::map< int, double > errorMap;

			for( IntPairDoubleMap::iterator it = linkErrors.begin(), end = linkErrors.end();
					it!=end;
					it++)
			{
				if(it->first.first >=0)
				{
					int thisLinkClusterID = clusterizer.getClusterID(it->first);
					errorMap[thisLinkClusterID]	+= it->second;
				}
			}// errorMap stores the combined chi2 error of eges for all clusters

			IntSet::iterator sIt = H.begin(), sEnd = H.end();

			double min_CI = 0;
			int rejectID = -1;

			for ( ; sIt!=sEnd; sIt++)
			{
				double CI = errorMap[*sIt]/utils::chi2(edgeDimension*clusterizer.getClusterByID(*sIt).size());

				if( CI >= min_CI ) // Just looking for the ones in H
				{
					min_CI = CI;
					rejectID = *sIt;
				}
			}
			H.erase(rejectID);
			RejectSet.insert(rejectID);

			return interClusterConsistent(H,goodSet,RejectSet);
		}
		return true;
	}

	bool robustify(bool eraseIncorrectLinks=false)
	{

		if(!gWrapper->isInitialized())
		{
			std::cerr<<" Please read in a graph file with read() or set the optimizer with setOptimizer()"<<std::endl;
			return false;
		}
		// First look for intra-cluster consistency

		IntSet
			consistentClusters,
			hypotheses,
			goodSet,
			rejectSet,
			tempRejectSet;

		
		std::cout<<"Number of Clusters found : "<<clusterizer.clusterCount()<<std::endl;
		std::cout<<"print all clusters: " << std::endl;
		for(size_t i=0; i< clusterizer.clusterCount(); i++)
        {
		    IntPairSet tempset = clusterizer.getClusterByID(i);
		    for (IntPairSet::iterator temppair = tempset.begin(); temppair != tempset.end(); temppair++)
            {
                std::cout<<i<<": " << temppair->first << " " << temppair->second << std::endl;
            }

        }
		std::cout<<"Checking Intra cluster consistency : "<<std::endl;
        auto start = std::chrono::steady_clock::now();
		for(size_t i=0; i< clusterizer.clusterCount(); i++)
		{
			std::cout<<i<<" "; std::cout.flush();
			if(intraClusterConsistent(i))
			{
				consistentClusters.insert(i);
			}
		}
		std::cout<<"done"<<std::endl<<std::endl;
        auto end = std::chrono::steady_clock::now();
        auto diff = end - start;
        std::cout << std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;
        std::cout << "" << std::endl;
		
		/// prints the consistent Clusters
		std::cout<<"Self-consistent clusters: "<<std::endl;
		for(int cluster_id : consistentClusters)
		{
			std::cout<< cluster_id <<" ";
		}
		std::cout<<"done"<<std::endl<<std::endl;

		/// ------------- consistentCluster are self-consistent --------- ////

        std::cout<<"Checking Inter cluster consistency : "<<std::endl;
        auto startt = std::chrono::steady_clock::now();

		bool done = false;
		while(!done)
		{
			done = true;

			IntSet::iterator
				cIt 	= consistentClusters.begin(),
				cEnd 	= consistentClusters.end();

			for( ; cIt!=cEnd; cIt++)
			{
				if(goodSet.find(*cIt)==goodSet.end() and
						rejectSet.find(*cIt)==rejectSet.end()) // We can't ignore this because it is nether in goodSet nor in rejectSet at the moment
				{
					hypotheses.insert(*cIt);
				}
			}

			IntPairSet activeLoops;
			gatherLinks(hypotheses,activeLoops);

			IntPairDoubleMap
				linkErrors =gWrapper->optimize(activeLoops,nIterations);


			hypotheses.clear();
			for ( IntPairDoubleMap::iterator it = linkErrors.begin(), end = linkErrors.end();
					it!=end;
					it++)
			{
				if(it->first.first < 0) continue;
				if( it->second < utils::chi2(edgeDimension))
				{
					hypotheses.insert(clusterizer.getClusterID(it->first));
					done = false;
				}
			}

			size_t goodSetSize = goodSet.size();

			interClusterConsistent(hypotheses,goodSet,tempRejectSet);

			hypotheses.clear();

			if(goodSet.size() > goodSetSize)
			{
				rejectSet.clear();
			}
			rejectSet.insert(tempRejectSet.begin(),tempRejectSet.end());
			tempRejectSet.clear();

		}
		std::cout<<" GoodSet :";
		for( IntSet::iterator it= goodSet.begin(), end = goodSet.end(); it!=end; it++)
		{
			std::cout<<(*it)<<" ";
		}
		std::cout<<std::endl;
        auto endd = std::chrono::steady_clock::now();
        auto difff = endd - startt;
        std::cout << std::chrono::duration <double, std::milli> (difff).count() << " ms" << std::endl;


		_goodSet.insert(goodSet.begin(),goodSet.end());

		if(eraseIncorrectLinks) removeIncorrectLoops();

		return true;
	}

	bool write(const char* filename)
	{
		IntPairSet correctLinks ;
		gatherLinks(_goodSet, correctLinks);
		gWrapper->write(filename,correctLinks);

		return true;

	}

	bool removeIncorrectLoops()
	{
		size_t count = clusterizer.clusterCount();

		IntSet rejected;

		for(size_t i = 0 ; i<count ; i++)
		{
			if(_goodSet.find(i)==_goodSet.end())
			{
				rejected.insert(i);
			}
		}
		rejected.insert(ID_IGNORE);

		IntPairSet rejectedLinks;

		gatherLinks(rejected,rejectedLinks);
		gWrapper->removeEdges(rejectedLinks);

		// Clear up the clusters from Clusterizer

		for( IntSet::const_iterator it = rejected.begin(), end = rejected.end(); it!=end; it++)
		{
			clusterizer.deleteCluster(*it);
		}

		return true;
	}

};




#endif /* RRR_HPP_ */

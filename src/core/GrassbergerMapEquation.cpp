/*
 * GrassbergerMapEquation.h
 */

#include "GrassbergerMapEquation.h"
#include "../utils/infomath.h"
#include "../utils/Log.h"
#include "Node.h"
#include <cmath>

namespace infomap {

	void GrassbergerMapEquation::initNetwork(NodeBase& root)
	{
		Log(3) << "GrassbergerMapEquation::initNetwork()...\n";
		m_totalDegree = 0;
		for (NodeBase& node : root)
		{
			m_totalDegree += getNode(node).data.flow;
		}
		double gamma = 0.5772156649015329;
		double log2 = 0.6931471805599453;
		m_grassbergerLog2.resize(m_totalDegree + 100);
		m_grassbergerLog2[0] = 0;
		m_grassbergerLog2[1] = (-gamma - log2) * M_LOG2E;
		m_grassbergerLog2[2] = (2.0 - gamma - log2) * M_LOG2E;

		for (unsigned int i = 3; i < m_totalDegree + 100; i += 2) {
			m_grassbergerLog2[i] = m_grassbergerLog2[i - 1];
			m_grassbergerLog2[i + 1] = (m_grassbergerLog2[i - 1] + 2.0 / i * M_LOG2E);
		}

		IntegerMapEquation::initNetwork(root);
	}

	double GrassbergerMapEquation::log2(double d) const
	{
		return infomath::log2(1.0 * d / m_totalDegree);
		//return m_grassbergerLog2[d];
	}

	double GrassbergerMapEquation::plogp(double d) const
	{
		// double p = d * 1.0 / m_totalDegree;
		// Log() << " (plogp(" << d << "/" << m_totalDegree << ") ";
		// return infomath::plogpN(d, m_totalDegree);
		return d == 0 ? 0 : d * 1.0 / m_totalDegree * ( log2(d) - log2(m_totalDegree) );
	}
	double GrassbergerMapEquation::plogpN(double d, double N) const
	{
		// return infomath::plogpN(d, m_totalDegree);
		return d == 0 ? 0 : d * 1.0 / N * ( log2(d) - log2(N) );
	}
}

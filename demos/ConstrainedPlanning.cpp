/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2014, Rice University
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Rice University nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Zachary Kingston */

#include "ConstrainedPlanningCommon.h"

/** Print usage information. Does not return. */
void usage(const char *const progname)
{
    std::cout << "Usage: " << progname << " -c <problem> -p <planner> -s <space> -t <timelimit> -w <sleep> -o\n";
    printProblems();
    printPlanners();
    exit(0);
}

enum SPACE
{
    ATLAS,
    PROJECTED
};

int main(int argc, char **argv)
{
    int c;
    opterr = 0;

    const char *plannerName = "RRTConnect";
    const char *problem = "sphere";
    const char *space = "projected";

    double artificalSleep = 0.0;
    double planningTime = 5.0;
    bool output = false;
    bool tb = true;
    bool printSpace = false;
    int iter = 0;

    unsigned int links = 5;
    unsigned int chains = 2;
    unsigned int simp = 1;
    unsigned int extra = 0;

    while ((c = getopt(argc, argv, "yg:c:p:s:w:ot:n:i:ax:e:")) != -1)
    {
        switch (c)
        {
            case 'y':
                printSpace = true;
                break;

            case 'c':
                problem = optarg;
                break;

            case 'e':
                extra = atoi(optarg);
                break;

            case 'x':
                simp = atoi(optarg);
                break;

            case 'g':
                chains = atoi(optarg);
                break;

            case 'a':
                tb = false;
                break;

            case 'i':
                iter = atoi(optarg);
                break;

            case 'p':
                plannerName = optarg;
                break;

            case 's':
                space = optarg;
                break;

            case 'w':
                artificalSleep = atof(optarg);
                break;

            case 't':
                planningTime = atof(optarg);
                break;

            case 'n':
                links = atoi(optarg);
                break;

            case 'o':
                output = true;
                break;

            default:
                usage(argv[0]);
                break;
        }
    }

    enum SPACE spaceType = PROJECTED;

    if (std::strcmp("atlas", space) == 0)
        spaceType = ATLAS;
    else if (std::strcmp("projected", space) == 0)
        spaceType = PROJECTED;
    // else if (std::strcmp("null", space) == 0)
    //     spaceType = NULLSPACE;
    else
    {
        std::cout << "Invalid constrained state space." << std::endl;
        usage(argv[0]);
    }

    Eigen::VectorXd x, y;
    ompl::base::StateValidityCheckerFn isValid;

    ompl::base::RealVectorBounds bounds(0);
    ompl::base::ConstraintPtr constraint(parseProblem(problem, x, y, isValid, bounds, artificalSleep, links, chains, extra));

    if (!constraint)
    {
        std::cout << "Invalid problem." << std::endl;
        usage(argv[0]);
    }

    printf("Constrained Planning Testing: \n"
           "  Planning with in `%s' state space with `%s' for `%s' problem.\n"
           "  Ambient Dimension: %u   CoDimension: %u\n"
           "  Timeout: %3.2fs   Artifical Delay: %3.2fs\n",
           space, plannerName, problem, constraint->getAmbientDimension(), constraint->getCoDimension(), planningTime,
           artificalSleep);


    ompl::base::StateSpacePtr rvss(new ompl::base::RealVectorStateSpace(constraint->getAmbientDimension()));
    rvss->as<ompl::base::RealVectorStateSpace>()->setBounds(bounds);

    ompl::base::ConstrainedStateSpacePtr css;
    ompl::geometric::SimpleSetupPtr ss;
    ompl::base::SpaceInformationPtr si;

    double range = 1;

    switch (spaceType)
    {
        case ATLAS:
        {
            ompl::base::AtlasStateSpacePtr atlas(new ompl::base::AtlasStateSpace(rvss, constraint));

            atlas->setSeparate(tb);

            ss = ompl::geometric::SimpleSetupPtr(new ompl::geometric::SimpleSetup(atlas));
            si = ss->getSpaceInformation();
            si->setValidStateSamplerAllocator(avssa);

            atlas->setSpaceInformation(si);

            // The atlas needs some place to start sampling from. We will make start and goal charts.
            ompl::base::AtlasChart *startChart = atlas->anchorChart(x);
            ompl::base::AtlasChart *goalChart = atlas->anchorChart(y);

            ompl::base::ScopedState<> start(atlas);
            ompl::base::ScopedState<> goal(atlas);
            start->as<ompl::base::AtlasStateSpace::StateType>()->vectorView() = x;
            start->as<ompl::base::AtlasStateSpace::StateType>()->setChart(startChart);
            goal->as<ompl::base::AtlasStateSpace::StateType>()->vectorView() = y;
            goal->as<ompl::base::AtlasStateSpace::StateType>()->setChart(goalChart);

            ss->setStartAndGoalStates(start, goal);

            css = atlas;
            break;
        }

        case PROJECTED:
        {
            ompl::base::ProjectedStateSpacePtr proj(new ompl::base::ProjectedStateSpace(rvss, constraint));
            ss = ompl::geometric::SimpleSetupPtr(new ompl::geometric::SimpleSetup(proj));
            si = ss->getSpaceInformation();
            si->setValidStateSamplerAllocator(pvssa);

            proj->setSpaceInformation(si);

            // The proj needs some place to start sampling from. We will make start
            // and goal charts.
            ompl::base::ScopedState<> start(proj);
            ompl::base::ScopedState<> goal(proj);
            start->as<ompl::base::ProjectedStateSpace::StateType>()->vectorView() = x;
            goal->as<ompl::base::ProjectedStateSpace::StateType>()->vectorView() = y;
            ss->setStartAndGoalStates(start, goal);

            css = proj;
            break;
        }
    }

    ss->setStateValidityChecker(isValid);

    // Choose the planner.
    ompl::base::PlannerPtr planner(parsePlanner(plannerName, si));
    if (!planner)
    {
        std::cout << "Invalid planner." << std::endl;
        usage(argv[0]);
    }

    ss->setPlanner(planner);

    css->registerProjection("sphere", ompl::base::ProjectionEvaluatorPtr(new SphereProjection(css)));
    css->registerProjection("chain", ompl::base::ProjectionEvaluatorPtr(new ChainProjection(css, 3, links)));
    css->registerProjection("stewart", ompl::base::ProjectionEvaluatorPtr(new StewartProjection(css, links, chains)));

    // Bounds
    double bound = 20;
    if (strcmp(problem, "chain") == 0)
        bound = links;

    try
    {
        if (strcmp(plannerName, "KPIECE1") == 0)
            planner->as<ompl::geometric::KPIECE1>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "BKPIECE1") == 0)
            planner->as<ompl::geometric::BKPIECE1>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "LBKPIECE1") == 0)
            planner->as<ompl::geometric::LBKPIECE1>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "ProjEST") == 0)
            planner->as<ompl::geometric::ProjEST>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "PDST") == 0)
            planner->as<ompl::geometric::PDST>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "SBL") == 0)
            planner->as<ompl::geometric::SBL>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "STRIDE") == 0)
            planner->as<ompl::geometric::STRIDE>()->setProjectionEvaluator(problem);
    }
    catch (std::exception &e)
    {
    }

    ss->setup();

    if (printSpace)
        ss->print(std::cout);

    std::clock_t tstart = std::clock();

    ompl::base::PlannerStatus stat;
    if (iter)
    {
        ompl::base::IterationTerminationCondition cond(iter);
        stat = planner->solve(cond);
        std::cout << cond.getTimesCalled() << "/" << iter << " iterations." << std::endl;
    }
    else
        stat = planner->solve(planningTime);

    if (stat)
    {
        const double time = ((double)(std::clock() - tstart)) / CLOCKS_PER_SEC;
        std::cout << "Took " << time << " seconds." << std::endl;

        ompl::geometric::PathGeometric &path = ss->getSolutionPath();

        double originalLength = path.length();
        for (unsigned int i = 0; i < simp; ++i)
            ss->simplifySolution();
        std::cout << "Path Length " << originalLength << " -> " << path.length() << std::endl;

        if (output)
        {
            std::cout << "Interpolating path..." << std::endl;
            path.interpolate(100);

            std::cout << "Dumping animation file..." << std::endl;
            std::ofstream animFile("anim.txt");
            path.printAsMatrix(animFile);
            animFile.close();

            if (x.size() == 3)
            {
                std::cout << "Dumping path mesh..." << std::endl;
                std::ofstream pathFile("path.ply");
                path.printPLY(pathFile);
                pathFile.close();

                std::cout << "Dumping graph mesh..." << std::endl;

                ompl::base::PlannerData data(si);
                planner->getPlannerData(data);

                std::ofstream graphFile("graph.ply");
                data.printPLY(graphFile, false);
                graphFile.close();
            }

            if (constraint->getManifoldDimension() == 2 && spaceType == ATLAS)
            {
                std::cout << "Dumping atlas mesh..." << std::endl;
                std::ofstream atlasFile("atlas.ply");
                css->as<ompl::base::AtlasStateSpace>()->printPLY(atlasFile);
                atlasFile.close();
            }
        }

        if (stat == ompl::base::PlannerStatus::APPROXIMATE_SOLUTION)
            std::cout << "Solution is approximate." << std::endl;
    }
    else
    {
        std::cout << "No solution found." << std::endl;
    }

    if (spaceType == ATLAS)
    {
        std::cout << "Atlas created " << css->as<ompl::base::AtlasStateSpace>()->getChartCount() << " charts."
                  << std::endl;
        std::cout << css->as<ompl::base::AtlasStateSpace>()->estimateFrontierPercent() << "% open." << std::endl;
    }

    return 0;
}

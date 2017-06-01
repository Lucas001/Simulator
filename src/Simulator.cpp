/*The MIT License (MIT)

Copyright (c) 2016 Lucas Borsatto Simão

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
*/

#include "Simulator.h"

Simulator::Simulator(){
    stratStep = 0;
	loopBullet = 0;
	numRobotsTeam = NUM_ROBOTS_TEAM;

	gameState = new GameState();
    runningPhysics = false;
}

void Simulator::runSimulator(int argc, char *argv[], ModelStrategy *stratBlueTeam, ModelStrategy *stratYellowTeam){
    int numTeams = 0;
	if(stratBlueTeam) {
		this->strategies.push_back(stratBlueTeam);
		numTeams++;
	}
	if(stratYellowTeam){
		this->strategies.push_back(stratYellowTeam);
		numTeams++;
	}

	if(numTeams == 0){
		cout << "You must set a strategy to run the simulator!" << endl;
		exit(1);
	}

	physics = new Physics(numTeams);
	HandleGraphics::initGraphics(physics,strategies,argc,argv);

    vector<RobotPhysics*> gRobots = physics->getAllRobots();

    for(int i = 0; i < physics->getNumTeams();i++){
        vector<RobotStrategy*> robotStrategiesTeam;
        for(int j = 0; j < numRobotsTeam;j++){
            RobotStrategy* robotStrategy = new RobotStrategy(0);
            robotStrategiesTeam.push_back(robotStrategy);
        }
        gameState->robotStrategiesTeam = robotStrategiesTeam;
        gameState->robotStrategiesAdv = robotStrategiesTeam;
    }

    pthread_t thread[3];
	pthread_create(&(thread[0]), NULL, &Simulator::runPhysics_thread, this);
    pthread_create(&(thread[1]), NULL, &Simulator::runGraphics_thread, this);
    pthread_create(&(thread[2]), NULL, &Simulator::runStrategies_thread, this);

    pthread_join(thread[0], NULL);
    pthread_join(thread[1], NULL);
    pthread_join(thread[2], NULL);
}

void* Simulator::runGraphics(){
    while(!runningPhysics){
        usleep(1000000.f*timeStep/handTime);
    }
	HandleGraphics::runGraphics();
}

void* Simulator::runPhysics(){
    int subStep = 1;
    float standStep = 1.f/60.f;

    while(!HandleGraphics::getScenario()->getQuitStatus()){
        usleep(1000000.f*timeStep/handTime);

        if(HandleGraphics::getScenario()->getSingleStep() && gameState->sameState ){
            cout << endl << endl << endl;
            loopBullet++;
            cout << "--------Ciclo Atual:\t" << loopBullet << "--------" << endl;
            physics->stepSimulation(timeStep,subStep,standStep);
            gameState->sameState = false;
            runningPhysics = true;
            HandleGraphics::getScenario()->setSingleStep(false);
        }

    }
    return(NULL);
}

void* Simulator::runStrategies(){
    btVector3 posTargets[] = {btVector3(SIZE_WIDTH,0,SIZE_DEPTH/2),btVector3(0,0,SIZE_DEPTH/2)};
    int attackDir = 0;
    int framesSec = (int)(1/timeStep);
    for(int i = 0; i < physics->getNumTeams();i++){
        if(posTargets[i].getX() > 0)  attackDir = 1;
        else attackDir = -1;
        strategies[i]->setAttackDir(attackDir);
        strategies[i]->setFramesSec(framesSec);

        for(int j = 0; j < numRobotsTeam;j++){
            int id = i*numRobotsTeam + j;
            physics->getAllRobots()[id]->setTimeStep(timeStep);
        }
            
    }
    while(!HandleGraphics::getScenario()->getQuitStatus()){
        usleep(1000000.f*timeStep/handTime);
        if(HandleGraphics::getScenario()->getSingleStep()){
            if(!gameState->sameState){
                updateWorld();

                /*Copy to not overload values in each strategy
                //--------------MÉTODO CUSTOSO PARA A CPU!! --------------
                vector<RobotStrategy*> cpRbStrategyTeam;
                vector<RobotStrategy*> cpRbStrategyAdv;
                for(int j = 0; j < numRobotsTeam;j++){
                    RobotStrategy* cpStrategy = new RobotStrategy(0);
                    *cpStrategy = *(gameState->robotStrategiesTeam.at(j));
                    cpRbStrategyTeam.push_back(cpStrategy);
                    *cpStrategy = *(gameState->robotStrategiesAdv.at(j));
                    cpRbStrategyAdv.push_back(cpStrategy);
                }*/

                if(strategies.size() > 0){
                    btVector3 ballPos = calcRelativePosition(physics->getBallPosition(),strategies[0]->getAttackDir());
                    calcRelativeWorld(gameState->robotStrategiesTeam,strategies[0]->getAttackDir());

                    strategies[0]->runStrategy(gameState->robotStrategiesTeam,gameState->robotStrategiesTeam,ballPos);
                    if(strategies.size() == 2){
                        btVector3 ballPos = calcRelativePosition(physics->getBallPosition(),strategies[1]->getAttackDir());
                        calcRelativeWorld(gameState->robotStrategiesAdv,strategies[1]->getAttackDir());
                        calcRelativeWorld(gameState->robotStrategiesTeam,strategies[0]->getAttackDir());
                        strategies[1]->runStrategy(gameState->robotStrategiesAdv,gameState->robotStrategiesTeam,ballPos);
                    }
                }else{
                    cout << "You must set a strategy to run the simulator!\n" << endl;
                    exit(0);
                }

                for(int i = 0; i < physics->getNumTeams(); i++){
                    for(int j = 0; j < numRobotsTeam;j++){
                        int id = i*numRobotsTeam + j;
                        if(strategies[i]->getAttackDir() == 1){
                            physics->getAllRobots()[id]->updateRobot(strategies[i]->getRobotStrategiesTeam()[j]->getCommand());
                        }
                        else{
                            float invCommand[2];
                            invCommand[0] = strategies[i]->getRobotStrategiesTeam()[j]->getCommand()[1];
                            invCommand[1] = strategies[i]->getRobotStrategiesTeam()[j]->getCommand()[0];

                            physics->getAllRobots()[id]->updateRobot(invCommand);
                        } 
                    }
                }

                gameState->sameState = true;
            }
        }
    }
    return(NULL);
}

void Simulator::updateWorld(){
    vector<RobotPhysics*> gRobots = physics->getAllRobots();

    for(int i = 0; i < physics->getNumTeams();i++){
        vector<RobotStrategy*> robotStrategiesTeam;
        for(int j = 0; j < numRobotsTeam;j++){
            RobotStrategy* robotStrategy;
            int idRobot = i*numRobotsTeam+j;
            robotStrategy = updateLocalPhysics(j, gRobots[idRobot]);
            robotStrategiesTeam.push_back(robotStrategy);
        }
        if(i == 0)  gameState->robotStrategiesTeam = robotStrategiesTeam;
        else    gameState->robotStrategiesAdv = robotStrategiesTeam;
    }
}

RobotStrategy* Simulator::updateLocalPhysics(int id, RobotPhysics* physicRobot){
    RobotStrategy* robotStrategy = new RobotStrategy(id);

    btTransform  transTemp;
    physicRobot->getRigidBody()->getMotionState()->getWorldTransform(transTemp);
    robotStrategy->setPosition(transTemp.getOrigin());

    btVector3 forwardVec = physicRobot->getRaycast()->getForwardVector();

    robotStrategy->setLocalFront(forwardVec);

    return robotStrategy;
}

void Simulator::calcRelativeWorld(vector<RobotStrategy*> robotStrategiesTeam,int attackDir){
        for(int i = 0; i < robotStrategiesTeam.size(); i++){
            robotStrategiesTeam[i]->setPosition(calcRelativePosition(robotStrategiesTeam[i]->getPosition(),attackDir));
            robotStrategiesTeam[i]->setLocalFront(attackDir*robotStrategiesTeam[i]->getLocalFront());
            robotStrategiesTeam[i]->setLocalRight(attackDir*robotStrategiesTeam[i]->getLocalRight());
        }
}

btVector3 Simulator::calcRelativePosition(btVector3 absPos, int attackDir){
    float relX = absPos.getX();
    float relZ = absPos.getZ();
    if(attackDir == -1){

        relZ = SIZE_DEPTH - absPos.getZ();
        relX = SIZE_WIDTH - absPos.getX();
    } 
    return btVector3(relX,0,relZ);
}

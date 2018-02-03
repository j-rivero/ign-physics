/*
 * Copyright (C) 2017 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <unordered_map>

#include <ode/ode.h>

#include "ignition/common/Console.hh"

#include "ODEDoublePendulum.hh"
#include "ODEMathConversions.hh"

namespace ignition
{
  namespace physics
  {
    namespace ode
    {
      struct ODEState
      {
        IGN_PHYSICS_DATA_LABEL(ignition::physics::ode::ODEState)

        // using StateMap =
        //     std::unordered_map<::dart::dynamics::SkeletonPtr,
        //                        ::dart::dynamics::Skeleton::Configuration>;

        StateMap states;
      };


      class PrivateODEDoublePendulum
      {
        public: dWorldID world;
        public: dSpaceID space;

        public: dBodyID body1;
        public: dBodyID body2;

        public: dJointID joint1;
        public: dJointID joint2;


        public: double dt = 1e-3;
        public: Eigen::VectorXd forces = Eigen::VectorXd::Zero(2);

        public: using BodyMap =
            std::unordered_map<std::size_t, ::dart::dynamics::WeakBodyNodePtr>;

        public: BodyMap mapToBodies;

        public: std::size_t lastId;

        PrivateODEDoublePendulum()
          : lastId(0)
        {
          // based on demo_hinge.cpp and demo_dhinge.cpp
          world = dWorldCreate();

          body1 = dBodyCreate(world);
          body2 = dBodyCreate(world);

          dBodySetPosition(body1, 0, 0.1, 1.95);
          dBodySetPosition(body2, 0, 0.2, 2.85);


          dGeomID g;
          dMass inertial;
          {
            const double mass = 1.0;
            const double ixx = 1.0;
            const double iyy = 1.0;
            const double izz = 1.0;
            const double cgz = 0.45;
            dMassSetParameters(&inertial,
              0, 0, cgz,
              ixx, iyy, izz,
              0, 0, 0);
          }

          g = dCreateBox(space, 0.1, 0.1, 1.0);
          dGeomSetBody(g, body1);
          dBodySetMass(body1, &inertial);

          g = dCreateBox(space, 0.1, 0.1, 1.0);
          dGeomSetBody(g, body2);
          dBodySetMass(body2, &inertial);

          // joint1 connected to world
          joint1 = dJointCreateHinge(world, 0);
          dJointAttach(joint1, body1, 0);
          dJointSetHingeAxis(joint1, 0, 1, 0);
          dJointSetHingeAnchor(joint1, 0, 0.1, 1.95);

          // joint2 connecting body1 and body2
          joint1 = dJointCreateHinge(world, 0);
          dJointAttach(joint2, body2, body1);
          dJointSetHingeAxis(joint2, 0, 1, 0);
          dJointSetHingeAnchor(joint2, 0, 0.2, 2.85);

          this->SetBodyMap();
          dWorldSetGravity(world, 0, 0, -9.81);
        }

        void SetBodyMap()
        {
          for (std::size_t i=0; i < this->world->getNumSkeletons(); ++i)
          {
            ::dart::dynamics::SkeletonPtr skel = this->world->getSkeleton(i);
            for (std::size_t j=0; j < skel->getNumBodyNodes(); ++j)
            {
              ::dart::dynamics::BodyNode * const bn = skel->getBodyNode(j);
              this->mapToBodies[lastId++] = bn;
            }
          }
        }

        void SetState(const SetState::State &x)
        {
          const DartState *state = x.Query<DartState>();
          if (!state)
          {
            ignerr << "[ignition::physics::dart::ODEDoublePendulum::"
                   << "SetState] The state provided does not contain a "
                   << "DartState, which this plugins needs in order to go to a "
                   << "specified state!\n";
            return;
          }

          std::unordered_set<::dart::dynamics::SkeletonPtr> allSkels;
          for (const auto &entry : state->states)
          {
            const ::dart::dynamics::SkeletonPtr &skel = entry.first;
            const ::dart::dynamics::SkeletonPtr &worldSkel =
                world->getSkeleton(skel->getName());
            if (skel != worldSkel)
            {
              world->removeSkeleton(worldSkel);
              world->addSkeleton(skel);
            }

            skel->setConfiguration(entry.second);

            allSkels.insert(skel);
          }

          std::unordered_set<::dart::dynamics::SkeletonPtr> removeSkels;
          for (std::size_t i=0; i < world->getNumSkeletons(); ++i)
          {
            const ::dart::dynamics::SkeletonPtr &worldSkel =
                this->world->getSkeleton(i);

            if (allSkels.count(worldSkel) == 0)
              removeSkels.insert(worldSkel);
          }

          for (const auto &skel : removeSkels)
            this->world->removeSkeleton(skel);
        }

        void WriteState(ForwardStep::State &x)
        {
          DartState &state = x.Get<DartState>();
          state.states.clear();

          for (std::size_t i=0; i < world->getNumSkeletons(); ++i)
          {
            const ::dart::dynamics::SkeletonPtr &skel =
                this->world->getSkeleton(i);

            state.states[skel] = skel->getConfiguration();
          }
        }

        void SetInputs(const GeneralizedParameters *_efforts)
        {
          if (_efforts != nullptr)
          {
            this->forces[_efforts->dofs[0]] = _efforts->forces[0];
            this->forces[_efforts->dofs[1]] = _efforts->forces[1];
          }
        }

        void SetTimeStep(const TimeStep *_timeStep)
        {
          if (_timeStep != nullptr)
          {
            this->dt = _timeStep->dt;
          }
        }

        void Simulate()
        {
          //this->robot->setForces(this->forces);

          dWorldStep(world, this->dt);
        }
      };

      ODEDoublePendulum::~ODEDoublePendulum()
      {
        // Do nothing
      }

      ODEDoublePendulum::ODEDoublePendulum()
        : dataPtr(new PrivateODEDoublePendulum)
      {
        // Do nothing
      }

      void ODEDoublePendulum::Step(
          Output &h, ForwardStep::State &x, const Input &u)
      {
        this->dataPtr->SetInputs(u.Query<GeneralizedParameters>());
        this->dataPtr->SetTimeStep(u.Query<TimeStep>());

        this->dataPtr->Simulate();

        this->dataPtr->WriteState(x);

        h.ResetQueries();
        this->WriteRequiredData(h);
        this->Write(h.Get<ignition::physics::JointPositions>());
      }

      void ODEDoublePendulum::SetStateTo(const SetState::State &x)
      {
        this->dataPtr->SetState(x);
      }

      void ODEDoublePendulum::Write(JointPositions &positions) const
      {
        auto configuration = this->dataPtr->robot->getConfiguration(
                              ::dart::dynamics::Skeleton::CONFIG_POSITIONS);
        positions.dofs = configuration.mIndices;
        positions.positions.clear();
        positions.positions.resize(this->dataPtr->robot->getNumDofs());

        for (const auto &dof : positions.dofs)
        {
          if (dof < this->dataPtr->robot->getNumDofs())
          {
            positions.positions[dof] = configuration.mPositions[dof];
          }
        }
      }

      void ODEDoublePendulum::Write(WorldPoses &poses) const
      {
        poses.entries.clear();
        poses.entries.reserve(this->dataPtr->mapToBodies.size());

        std::vector<std::size_t> cleanup;
        for(const auto &entry : this->dataPtr->mapToBodies)
        {
          const std::size_t id = entry.first;
          const ::dart::dynamics::BodyNodePtr bn = entry.second.lock();
          if(!bn)
          {
            // Remove bodies that no longer exist
            cleanup.push_back(id);
            continue;
          }

          WorldPose wp;
          //wp.pose = convert(bn->getWorldTransform());
          wp.body = id;

          poses.entries.push_back(wp);
        }

        for(const std::size_t id : cleanup)
          this->dataPtr->mapToBodies.erase(id);
      }
    }
  }
}
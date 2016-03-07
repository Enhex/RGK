## ======================================================================== ##
## Copyright 2009-2013 Intel Corporation                                    ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

FIND_PATH(embree_INCLUDE_DIRS embree2/rtcore.h
  /usr/include
  /usr/local/include
  /opt/local/include)

FIND_LIBRARY(embree_LIBRARY NAMES embree PATHS
  /usr/lib
  /usr/local/lib
  /opt/local/lib)

FIND_LIBRARY(embree_LIBRARY_MIC NAMES embree_xeonphi PATHS
  /usr/lib
  /usr/local/lib
  /opt/local/lib)

IF (embree_INCLUDE_DIRS AND embree_LIBRARY)
  SET(embree_FOUND TRUE)
  message(STATUS "Found embree: " ${embree_INCLUDE_DIRS})
ELSE()
  if (embree_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find embree library")
  endif (embree_FIND_REQUIRED)
ENDIF ()
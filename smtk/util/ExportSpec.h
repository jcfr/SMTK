/*=========================================================================

Copyright (c) 2013 Kitware Inc. 28 Corporate Drive,
Clifton Park, NY, 12065, USA.

All rights reserved. No part of this software may be reproduced, distributed,
or modified, in any form or by any means, without permission in writing from
Kitware Inc.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN
"AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO
PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
=========================================================================*/
// .NAME ExportSpec - Application data for passing to python scripts.
// .SECTION Description
// This class if for storing application data passed from application code
// to python scripts.
// .SECTION See Also


#ifndef __smtk_util_ExportSpec_h
#define __smtk_util_ExportSpec_h

#include "smtk/SMTKCoreExports.h"
#include "smtk/PublicPointerDefs.h"
#include "smtk/util/Logger.h"

#include <string>
#include <vector>

namespace smtk
{
  namespace util
  {
    class SMTKCORE_EXPORT ExportSpec
    {
    public:
      // ----------------------------------------
      // Data-get methods, intended to be called from python scripts
      smtk::attribute::Manager *getSimulationAttributes() const
      { return m_simulationManager; }
      smtk::attribute::Manager *getExportAttributes() const
      { return m_exportManager; }
      smtk::model::GridInfoPtr getAnalysisGridInfo() const
      { return m_analysisGridInfo; }
      smtk::util::Logger getLogger() const
      { return m_logger; }

      // ----------------------------------------
      // Constructor and data-set methods, intended to be called from C/C++ code
      ExportSpec();
      void clear();

      void setSimulationAttributes(smtk::attribute::Manager *manager)
      { m_simulationManager = manager; }
      void setExportAttributes(smtk::attribute::Manager *manager)
      { m_exportManager = manager; }
      void setAnalysisGridInfo(smtk::model::GridInfoPtr analysisGridInfo)
      { m_analysisGridInfo = analysisGridInfo; }

    private:
      smtk::attribute::Manager *m_simulationManager;
      smtk::attribute::Manager *m_exportManager;
      smtk::model::GridInfoPtr  m_analysisGridInfo;
      smtk::util::Logger        m_logger;
    };
  }
}


#endif  /* __smtk_util_ExportSpec_h */

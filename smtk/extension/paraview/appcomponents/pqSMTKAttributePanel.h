//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#ifndef smtk_extension_paraview_appcomponents_pqSMTKAttributePanel_h
#define smtk_extension_paraview_appcomponents_pqSMTKAttributePanel_h

#include "smtk/extension/paraview/appcomponents/smtkPQComponentsExtModule.h"

#include "smtk/extension/qt/qtUIManager.h"

#include "smtk/resource/Observer.h"

#include "smtk/PublicPointerDefs.h"

#include "pqPropertyLinks.h"

#include <QDockWidget>

class pqServer;
class pqPipelineSource;

/**\brief A panel that displays a single SMTK resource for editing by the user.
  *
  * Because attribute resources currently ship with a top-level view (implied
  * if none exists), we always display that view when asked to show a resource.
  * This may change in the future.
  *
  * This panel will create a new SMTK attribute UI manager each time the
  * resource to be displayed is switched for a different resource.
  */
class SMTKPQCOMPONENTSEXT_EXPORT pqSMTKAttributePanel : public QDockWidget
{
  Q_OBJECT
  typedef QDockWidget Superclass;

public:
  pqSMTKAttributePanel(QWidget* parent = nullptr);
  ~pqSMTKAttributePanel() override;

  smtk::extension::qtUIManager* attributeUIManager() const { return m_attrUIMgr; }

public slots:
  /**\brief Populate the attribute panel with data from \a psrc
    *
    * If \a psrc is an attribute source, then call displayResource()
    * on the resource.
    */
  virtual bool displayPipelineSource(pqPipelineSource* psrc);
  /**\brief Populate the attribute panel with data from \a rsrc.
    *
    * Look up \a rsrc's top-level view and call displayView() with it.
    */
  virtual bool displayResource(const smtk::attribute::ResourcePtr& rsrc);
  /**\brief Populate the attribute panel with data from \a rsrc.
    *
    * Look up \a rsrc's top-level view and call displayView() with it.
    * This variant does more than displayResource() alone;
    * it will obtain the wrapper associated with the resource's manager
    * and use it for selection as displayPipelineSource() does before
    * calling the plain displayResource() variant.
    */
  virtual bool displayResourceOnServer(const smtk::attribute::ResourcePtr& rsrc);
  /**\brief Populate the attribute panel with the given view.
    *
    * Note that the \a view should describe an attribute resource.
    * It is possible for views to describe other things, such as the
    * configuration of a descriptive-phrase tree, but those will not
    * be accepted by this method.
    */
  virtual bool displayView(smtk::view::ConfigurationPtr view);
  /**\brief Update the attribute panel when the ParaView pipeline changes.
    *
    * The attribute resource associated with the active
    * ParaView pipeline source may have changed (i.e., the filename changed
    * and so the old resource is dropped and a new one constructed) or been
    * updated (i.e., the resource location has not changed, but the resource
    * contents have changed). Re-render.
    */
  virtual bool updatePipeline();
  /**\brief Clear panel widgets, unobserve displayed resource,
    *       and set the attribute resource pointer to null.
    */
  virtual void resetPanel(smtk::resource::ManagerPtr rsrcMgr);

protected slots:
  /**\brief Called when vtkSMTKSettings is modified, indicating user preferences have changed.
    *
    * The attribute panel listens for changes to the highlight-on-hover
    * preference as well as default-value and error-value colors.
    * This slot notifies the qtUIManager when these have changed
    * and it in turn emits a signal that views or items can listen to
    * for updates.
    */
  virtual void updateSettings();

  /**\brief Connect/disconnect signals+slots controlling synchronization of
   *        ParaView pipeline sources and the attribute panel.
   *
   * This slot is attached to pqSMTKBehavior::postProcessingModeChanged()
   * so that the connections only exist during post-processing (when the
   * pipeline browser is visible).
   */
  virtual void displayActivePipelineSource(bool doDisplay);

protected:
  virtual bool displayResourceInternal(const smtk::attribute::ResourcePtr& rsrc);
  virtual void updateTitle(const smtk::view::ConfigurationPtr& view = nullptr);
  smtk::extension::qtUIManager* m_attrUIMgr{ nullptr };
  std::weak_ptr<smtk::resource::Resource> m_rsrc;
  smtk::view::SelectionPtr m_seln;
  smtk::view::ManagerPtr m_viewManager;
  smtk::operation::ManagerPtr m_opManager;
  smtk::resource::Observers::Key m_observer;
  pqPropertyLinks m_propertyLinks;
};

#endif // smtk_extension_paraview_appcomponents_pqSMTKAttributePanel_h

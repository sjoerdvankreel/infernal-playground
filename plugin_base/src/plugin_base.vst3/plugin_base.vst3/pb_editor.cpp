#include <plugin_base/shared/logger.hpp>
#include <plugin_base.vst3/pb_editor.hpp>
#if (defined __linux__) || (defined  __FreeBSD__)
#include <juce_events/native/juce_EventLoopInternal_linux.h>
#endif

using namespace juce;
using namespace Steinberg;

namespace plugin_base::vst3 {

pb_editor::
pb_editor(pb_controller* controller, std::vector<modulation_output>* modulation_outputs) :
EditorView(controller), _controller(controller),
_gui(std::make_unique<plugin_gui>(&controller->automation_state(), &controller->extra_state_(), modulation_outputs)) {}

tresult PLUGIN_API
pb_editor::getSize(ViewRect* new_size)
{
  new_size->right = new_size->left + _gui->getWidth();
  new_size->bottom = new_size->top + _gui->getHeight();
  checkSizeConstraint(new_size);
  return kResultTrue;
}

tresult PLUGIN_API
pb_editor::setContentScaleFactor(ScaleFactor factor)
{
#ifdef __APPLE__
  return kResultFalse;
#else
  ViewRect new_size;
  _gui->set_system_dpi_scale(factor);
  getSize(&new_size);
  if(plugFrame) plugFrame->resizeView(this, &new_size);
  return kResultTrue;
#endif
}

tresult PLUGIN_API
pb_editor::onSize(ViewRect* new_size)
{
  checkSizeConstraint(new_size);
  _gui->setSize(new_size->getWidth(), new_size->getHeight());
  return kResultTrue;
}

tresult PLUGIN_API
pb_editor::checkSizeConstraint(ViewRect* new_size)
{
  if (!_gui.get()) return kResultTrue;
  auto settings = _gui->get_lnf()->global_settings();
  auto const& topo = *_controller->automation_state().desc().plugin;
  bool is_fx = topo.type == plugin_type::fx;
  int min_width = (int)(settings.get_default_width(is_fx) * settings.min_scale * _gui->get_system_dpi_scale());
  int max_width = (int)(settings.get_default_width(is_fx) * settings.max_scale * _gui->get_system_dpi_scale());
  int new_width = std::clamp((int)new_size->getWidth(), min_width, max_width);
  new_size->right = new_size->left + new_width;
  new_size->bottom = new_size->top + (new_width * settings.get_aspect_ratio_height(is_fx) / settings.get_aspect_ratio_width(is_fx));
  return kResultTrue;
}

#if (defined __linux__) || (defined  __FreeBSD__)
void PLUGIN_API
pb_editor::onFDIsSet(Steinberg::Linux::FileDescriptor fd)
{ LinuxEventLoopInternal::invokeEventLoopCallbackForFd(fd); }
#endif

tresult PLUGIN_API 
pb_editor::isPlatformTypeSupported(FIDString type)
{
  PB_LOG_FUNC_ENTRY_EXIT();
#if WIN32
  return strcmp(type, kPlatformTypeHWND) == 0? kResultTrue: kResultFalse;
#elif (defined __APPLE__)
    return strcmp(type, kPlatformTypeNSView) == 0? kResultTrue: kResultFalse;
#elif (defined __linux__) || (defined  __FreeBSD__)
  return strcmp(type, kPlatformTypeX11EmbedWindowID) == 0 ? kResultTrue : kResultFalse;
#else
#error
#endif
}

tresult PLUGIN_API
pb_editor::removed()
{
  PB_LOG_FUNC_ENTRY_EXIT();
  _gui->remove_param_listener(_controller);
  _gui->setVisible(false);
  _gui->removeFromDesktop();
#if (defined __linux__) || (defined  __FreeBSD__)
  Steinberg::Linux::IRunLoop* loop = {};
  PB_ASSERT_EXEC(!plugFrame->queryInterface(Steinberg::Linux::IRunLoop::iid, (void**)&loop));
  loop->unregisterEventHandler(this);
#endif
  return EditorView::removed();
}

tresult PLUGIN_API
pb_editor::attached(void* parent, FIDString type)
{
  PB_LOG_FUNC_ENTRY_EXIT();
#if (defined __linux__) || (defined  __FreeBSD__)
  Steinberg::Linux::IRunLoop* loop = {};
  PB_ASSERT_EXEC(!plugFrame->queryInterface(Steinberg::Linux::IRunLoop::iid, (void**)&loop));
  for (int fd: LinuxEventLoopInternal::getRegisteredFds())
    loop->registerEventHandler(this, fd);
#endif
  _gui->addToDesktop(0, parent);
  _gui->getPeer()->setCurrentRenderingEngine(0);
  _gui->setVisible(true);
  _gui->add_param_listener(_controller);
  _gui->reloaded();
  return EditorView::attached(parent, type);
}

tresult PLUGIN_API
pb_editor::queryInterface(TUID const iid, void** obj)
{
#if (defined __linux__) || (defined  __FreeBSD__)
  QUERY_INTERFACE(iid, obj, Steinberg::Linux::IEventHandler::iid, Steinberg::Linux::IEventHandler)
#endif
  QUERY_INTERFACE(iid, obj, Steinberg::IPlugViewContentScaleSupport::iid, Steinberg::IPlugViewContentScaleSupport);
  return EditorView::queryInterface(iid, obj);
}

}

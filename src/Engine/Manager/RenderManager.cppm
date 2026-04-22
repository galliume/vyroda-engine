/**
 * @file   RenderManager.cppm
 * @author  <Guillaume PERCEPIED>
 * @date   Sun Mar  8 16:55:46 2026
 * 
 * @brief  Serves as the main controller
 * @todo trim some fat here and delegate more
 * 
 */
export module Engine.Managers.RenderManager;

import std;

import Engine.Component.Entity;
import Engine.Component.EntityNode;
import Engine.Component.Mesh;

import Engine.Core.Camera;
import Engine.Core.GLM;
import Engine.Core.LightTypes;
import Engine.Core.MeshTypes;
import Engine.Core.PlpTypedef;
import Engine.Core.Tools;
import Engine.Core.Volk;
import Engine.GUI.Window;

import Engine.Managers.AudioManager;
import Engine.Managers.ComponentManager;
import Engine.Managers.DestroyManager;
import Engine.Managers.EntityManager;
import Engine.Managers.FontManager;
import Engine.Managers.LightManager;
import Engine.Managers.PlayerManager;
import Engine.Managers.ShaderManager;
import Engine.Managers.TextureManager;

import Engine.Renderer.RendererComponentFactory;
import Engine.Renderer;
import Engine.Renderer.RendererComponentTypes;

import Engine.Utils.IDHelper;

namespace Vyroda
{
  /**
   * @struct Framecontext
   * @brief hold the current frame context
  **/
  struct FrameContext
  {
    double delta_time{};
    std::string root_path{};
    std::uint8_t camera_index{ 0 };

    glm::mat4 camera_view_matrix{ 1.0f };
    glm::mat4 perspective{ 1.0f };

    std::vector<glm::vec4> frustum_planes{};
    std::vector<IDType> visible_ids{};
    std::vector<IDType> transparent_ids{};

    std::shared_ptr<Entity> skybox_entity{};
    std::shared_ptr<Entity> terrain_entity{};
    std::shared_ptr<Entity> water_entity{};

    std::span<std::shared_ptr<Entity>> texts{};
  };

  export class RenderManager
  {
  public:
    /** 
     * Clean up
     *
     */
    void cleanUp();

    /** 
     * Force a refresh
     *
     */
    inline void forceRefresh() { _refresh = true; }

    /** 
     * Get application height
     *
     * @return 
     */
    inline std::uint32_t getAppHeight() { return getRenderer().getAPI().getSwapChainExtent().height; }

    /** 
     * Get application width
     *
     * @return 
     */
    inline std::uint32_t getAppWidth() { return getRenderer().getAPI().getSwapChainExtent().width; }

    /** 
     * Get AudioManager
     *
     * @return Audiomanager&
     */
    inline AudioManager& getAudioManager() { return *_audio_manager; }

    /** 
     * Get current camera
     *
     * @return Camera& 
     */
    inline Camera& getCamera() const { return *_cameras.at(_current_camera); }

    /** 
     * Get ComponentManager
     *
     * @return Componentmanager& 
     */
    inline ComponentManager& getComponentManager() { return *_component_manager; }

    /** 
     * Get Destroymanager
     *
     * @return Destroymanager& 
     */
    inline DestroyManager& getDestroyManager() { return *_destroy_manager; }

    /** 
     * Get EntityManager
     *
     * @return Entitymanager& 
     */
    inline EntityManager& getEntityManager() { return *_entity_manager; }

    /** 
     * Get LightManager
     *
     * @return LightManager& 
     */
    inline LightManager& getLightManager() { return *_light_manager; }

    /** 
     * Get Renderer
     *
     * @return Renderer& 
     */
    inline Renderer& getRenderer() { return *_renderer; }

    /** 
     * Get ShaderManager
     *
     * @return ShaderManager& 
     */
    inline ShaderManager& getShaderManager() { return *_shader_manager; }

    /** 
     * Get FontManager
     *
     * @return FontManager& 
     */
    inline FontManager& getFontManager() { return *_font_manager; }

    /** 
     * Get TextureManager
     *
     * @return TextureManager& 
     */
    inline TextureManager& getTextureManager() { return *_texture_manager; }

    /** 
     * Get Window
     *
     * @return Window& 
     */
    Window& getWindow();

    /** 
     * Init the RenderManager
     * Load the configuration, audio, input and load current level
     * @todo does too much ?
     */
    void init(std::unique_ptr<Window> window);

    /** 
     * Get if the level is loaded
     *
     * @return bool
     */
    inline bool isLoaded()  { return _is_loaded; }

    /** 
     * Render the scene
     *
     * @param double delta_time 
     */
    void renderScene(double const delta_time);

    /** 
     * Set the is loaded flag
     *
     * @param bool loaded 
     */
    inline void setIsLoaded(bool loaded = true) { _is_loaded = loaded; }

    /** 
     * Add text
     *
     * @param FontManager::Text& text 
     */
    void addText(FontManager::Text & text);

    /** 
     * Update a text
     *
     * @param IDType entity_id 
     * @param std::string text
     * @todo move ?
     */
    void updateText(IDType const entity_id, std::string const& text);

    /** 
     * Update text color
     *
     * @param IDType entity_id 
     * @param glm::vec3 color 
     */
    void updateTextColor(IDType const entity_id, glm::vec3 const& color);

    /** 
     * Set elapsed time
     *
     * @param double elapsed_time 
     */
    void setElapsedTime(double const elapsed_time) { _elapsed_time = elapsed_time;}

    /** 
     * Get elapsed time
     *
     * @return double 
     */
    double getElapsedTime() const { return _elapsed_time; }

    /** 
     * Get LightBuffer
     *
     * @return Buffer
     */
    Buffer getLightBuffer();

    /** 
     * Get Renderer Context
     *
     * @param glm::mat4 camera_view 
     *
     * @return RenderContext&
     */
    RendererContext& getRendererContext(glm::mat4 const& camera_view = glm::mat4(1.0f));

    /** 
     * Update renderer context
     *
     * @param glm::mat4 camera_view 
     */
    void updateRendererContext(glm::mat4 const& camera_view);

  private:
    void loadData(std::string const & level);

    bool isClipped(
    IDType const entity_id,
    std::span<glm::vec4 const> frustum_planes) const;

  void renderEntity(
    IDType const entity_id,
    double const delta_time,
    glm::mat4 const& camera_view_matrix);

  void drawEntity(
    IDType const entity_id,
    glm::mat4 const& camera_view_matrix,
    bool const has_alpha_blend = false);

  void drawShadowMap(
    IDType const entity_id,
    SHADOW_TYPE const shadow_type,
    glm::mat4 const& camera_view_matrix,
    bool const has_alpha_blend = false);

  auto get_visible_ids(
    std::ranges::range auto const& entities,
    std::vector<glm::vec4> const& frustum_planes) const
    {
      return entities
        | std::views::filter([&](auto const& e) { return e->isVisible() && !isClipped(e->getID(), frustum_planes); })
          | std::views::transform([](auto const& e) { return e->getID(); })
          | std::ranges::to<std::vector>();
    }

    //@todo move to EntityManager
    void prepareSkybox();
    void prepareTerrain();
    void prepareWater();

    FrameContext buildFrameContext(
      std::string const& root_path,
      std::uint8_t const camera_index,
      double const delta_time) const;

    void updateEntities(FrameContext const& frame_context);
    void renderShadowmap(FrameContext const& frame_context);
    void renderEntities(FrameContext const& frame_context);
    void updateBuffers();
    void updatePlayer();

  private:
    std::string _current_level;
    std::string _current_skybox;
    std::uint32_t _env_options {0};

    bool _is_loaded{ false };
    bool _refresh{ false };
    double _elapsed_time{ 0.0 };
    std::vector<std::unique_ptr<Camera>> _cameras;
    std::uint8_t _current_camera { 0 };
    std::unique_ptr<Renderer> _renderer;
    std::unique_ptr<Window> _window;

    std::unique_ptr<AudioManager> _audio_manager;
    std::unique_ptr<ComponentManager> _component_manager;
    std::unique_ptr<DestroyManager> _destroy_manager;
    std::unique_ptr<EntityManager> _entity_manager;
    std::unique_ptr<FontManager> _font_manager;
    std::unique_ptr<LightManager> _light_manager;
    std::unique_ptr<PlayerManager> _player_manager;
    std::unique_ptr<ShaderManager> _shader_manager;
    std::unique_ptr<TextureManager> _texture_manager;

    std::vector<VkDescriptorPool> _descriptor_pools;

    std::chrono::time_point<std::chrono::system_clock> _last_reload;

    std::vector<Buffer> _light_buffers;

    RendererContext _rendering_context;

    std::vector<IDType> _all_ids{};
  };
}

/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#include <gtk/gtk.h>


void
on_rocket_launcher_properties_dialog_destroy
                                        (GtkObject       *object,
                                        gpointer         user_data);

void
on_rocket_launcher_texture_pixmapentry_activate
                                        (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_rocket_launcher_texture_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_rocket_launcher_rockets_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_rocket_launcher_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_rocket_launcher_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_rocket_launcher_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_rocket_launcher_respawn_delay_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_speedup_ramp_properties_dialog_destroy
                                        (GtkObject       *object,
                                        gpointer         user_data);

void
on_speedup_ramp_texture_pixmapentry_activate
                                        (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_speedup_ramp_texture_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_speedup_ramp_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_speedup_ramp_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_speedup_ramp_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_speedup_ramp_height_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_speedup_ramp_height_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_speedup_ramp_height_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_speedup_ramp_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_speedup_ramp_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_speedup_ramp_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_speedup_ramp_activation_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_speedup_ramp_boost_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_minigun_properties_dialog_destroy   (GtkObject       *object,
                                        gpointer         user_data);

void
on_minigun_texture_pixmapentry_activate
                                        (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_minigun_texture_checkbutton_toggled (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_minigun_bullets_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_minigun_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_minigun_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_minigun_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_minigun_respawn_delay_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_plasma_cannon_properties_dialog_destroy
                                        (GtkObject       *object,
                                        gpointer         user_data);

void
on_plasma_cannon_texture_pixmapentry_activate
                                        (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_plasma_cannon_texture_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_plasma_cannon_plasmas_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_plasma_cannon_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_plasma_cannon_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_plasma_cannon_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_plasma_cannon_respawn_delay_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_rails_properties_dialog_destroy     (GtkObject       *object,
                                        gpointer         user_data);

void
on_rails_texture_pixmapentry_activate  (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_rails_texture_checkbutton_toggled   (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_rails_quantity_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_rails_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_rails_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_rails_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_rails_respawn_delay_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_shield_energy_properties_dialog_destroy
                                        (GtkObject       *object,
                                        gpointer         user_data);

void
on_shield_energy_texture_pixmapentry_activate
                                        (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_shield_energy_texture_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_shield_energy_energy_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_shield_energy_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_shield_energy_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_shield_energy_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_shield_energy_respawn_delay_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_spawn_point_properties_dialog_destroy
                                        (GtkObject       *object,
                                        gpointer         user_data);

void
on_spawn_point_texture_pixmapentry_activate
                                        (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_spawn_point_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_spawn_point_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_spawn_point_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_spawn_point_height_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_spawn_point_height_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_spawn_point_height_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_spawn_point_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_spawn_point_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_spawn_point_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_spawn_point_texture_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_spawn_point_teleport_only_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_spawn_point_index_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_gravity_well_properties_dialog_destroy
                                        (GtkObject       *object,
                                        gpointer         user_data);

void
on_gravity_well_texture_pixmapentry_activate
                                        (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_gravity_well_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_gravity_well_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_gravity_well_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_gravity_well_height_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_gravity_well_height_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_gravity_well_height_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_gravity_well_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_gravity_well_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_gravity_well_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_gravity_well_texture_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_gravity_well_strength_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_gravity_well_enclosed_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_teleporter_properties_dialog_destroy
                                        (GtkObject       *object,
                                        gpointer         user_data);

void
on_teleporter_texture_pixmapentry_activate
                                        (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_teleporter_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_teleporter_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_teleporter_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_teleporter_height_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_teleporter_height_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_teleporter_height_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_teleporter_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_teleporter_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_teleporter_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_teleporter_texture_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_teleporter_radius_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_teleporter_sparkles_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_teleporter_spawn_point_index_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_teleporter_rotate_type_abs_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_teleporter_rotate_type_rel_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_teleporter_rotation_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_teleporter_divider_angle_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_teleporter_divider_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_door_switch_properties_dialog_destroy
                                        (GtkObject       *object,
                                        gpointer         user_data);

void
on_door_colorpicker_color_set          (GtkColorButton  *colorpicker,
						gpointer         user_data);

void
on_door_width_spinbutton_value_changed (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_door_energy_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_door_initial_state_open_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_door_initial_state_closed_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_door_open_timeout_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_door_close_timeout_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_door_index_spinbutton_value_changed (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_door_checkbutton_toggled            (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_switch_colorpicker_color_set        (GtkColorButton  *colorpicker,
						gpointer         user_data);

void
on_switch_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_switch_on_enter_close_list_entry_changed
                                        (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_switch_on_enter_open_list_entry_changed
                                        (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_switch_on_enter_invert_list_entry_changed
                                        (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_switch_on_leave_close_list_entry_changed
                                        (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_switch_on_leave_open_list_entry_changed
                                        (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_switch_on_leave_invert_list_entry_changed
                                        (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_switch_checkbutton_toggled          (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_fill_properties_dialog_destroy      (GtkObject       *object,
                                        gpointer         user_data);

void
on_fill_solid_colorpicker_color_set    (GtkColorButton  *colorpicker,
						gpointer         user_data);

void
on_fill_solid_radiobutton_toggled      (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_fill_texture_pixmapentry_activate   (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_fill_flip_horiz_checkbutton_toggled (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_fill_flip_vert_checkbutton_toggled  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_fill_rotate_checkbutton_toggled     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_fill_rotate_left_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_fill_rotate_right_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_fill_stretch_horiz_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_fill_stretch_horiz_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_fill_stretch_horiz_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_fill_stretch_vert_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_fill_stretch_vert_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_fill_stretch_vert_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_fill_offset_horiz_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_fill_offset_horiz_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_fill_offset_horiz_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_fill_offset_vert_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_fill_offset_vert_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_fill_offset_vert_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_fill_texture_radiobutton_toggled    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_fill_friction_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_wall_properties_dialog_destroy      (GtkObject       *object,
                                        gpointer         user_data);

void
on_wall_solid_colorpicker_color_set    (GtkColorButton  *colorpicker,
						gpointer         user_data);

void
on_wall_solid_radiobutton_toggled      (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wall_texture_pixmapentry_activate   (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_wall_flip_horiz_checkbutton_toggled (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wall_flip_vert_checkbutton_toggled  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wall_rotate_checkbutton_toggled     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wall_rotate_left_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wall_rotate_right_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wall_offset_horiz_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_wall_offset_horiz_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_wall_offset_horiz_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_wall_offset_vert_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_wall_offset_vert_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_wall_offset_vert_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_wall_fixed_reps_radiobutton_toggled (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wall_fixed_reps_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_wall_fixed_reps_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_wall_fixed_reps_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_wall_fixed_width_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wall_fixed_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_wall_fixed_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_wall_fixed_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_wall_texture_radiobutton_toggled    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wall_width_lock_checkbutton_toggled (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wall_width_lock_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_wall_width_lock_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_wall_width_lock_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_end_node_properties_dialog_destroy  (GtkObject       *object,
                                        gpointer         user_data);

void
on_end_node_texture_pixmapentry_activate
                                        (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_end_node_flip_horiz_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_end_node_flip_vert_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_end_node_rotate_checkbutton_toggled (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_end_node_rotate_left_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_end_node_rotate_right_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_end_node_left_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_end_node_left_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_end_node_left_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_end_node_right_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_end_node_right_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_end_node_right_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_end_node_texture_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_end_node_magnitude_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_end_node_magnitude_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_end_node_magnitude_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_end_node_wall_left_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_end_node_wall_left_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_end_node_wall_left_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_end_node_wall_right_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_end_node_wall_right_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_end_node_wall_right_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_end_node_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_end_node_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_end_node_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_crossover_node_properties_dialog_destroy
                                        (GtkObject       *object,
                                        gpointer         user_data);

void
on_crossover_node_blend_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_crossover_node_texture_pixmapentry_activate
                                        (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_crossover_node_flip_horiz_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_crossover_node_flip_vert_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_crossover_node_rotate_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_crossover_node_rotate_left_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_crossover_node_rotate_right_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_crossover_node_texture_radiobutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_crossover_node_axis1_mag_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis1_mag_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis1_mag_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_crossover_node_axis1_left_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis1_left_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis1_left_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_crossover_node_axis1_right_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis1_right_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis1_right_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_crossover_node_axis2_mag_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis2_mag_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis2_mag_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_crossover_node_axis2_left_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis2_left_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis2_left_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_crossover_node_axis2_right_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis2_right_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_crossover_node_axis2_right_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_crossover_node_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_crossover_node_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_crossover_node_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_straight_through_node_properties_dialog_destroy
                                        (GtkObject       *object,
                                        gpointer         user_data);

void
on_straight_through_node_magnitude_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_straight_through_node_magnitude_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_straight_through_node_magnitude_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_straight_through_node_wall_left_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_straight_through_node_wall_left_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_straight_through_node_wall_left_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_straight_through_node_wall_right_width_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_straight_through_node_wall_right_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_straight_through_node_wall_right_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_straight_through_node_angle_spinbutton_value_changed
                                        (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

gboolean
on_straight_through_node_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_straight_through_node_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_wall_texture_pixmapentry_activate   (GtkWidget *gnomefileentry,
                                        gpointer         user_data);

void
on_wall_texture_entry_activate         (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_wall_texture_entry_changed          (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_rocket_launcher_texture_entry_changed
						(GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_speedup_ramp_texture_entry_changed  (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_minigun_texture_entry_changed       (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_plasma_cannon_texture_entry_changed (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_rails_texture_pixmapentry_changed   (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_shield_energy_texture_entry_changed (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_spawn_point_texture_entry_changed   (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_gravity_well_texture_entry_changed  (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_teleporter_texture_entry_changed    (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_fill_texture_entry_changed          (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_node_texture_entry_changed          (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_end_node_texture_entry_changed      (GtkFileChooserButton *chooser,
						gpointer         user_data);

void
on_crossover_node_texture_entry_changed
						(GtkFileChooserButton *chooser,
						gpointer         user_data);

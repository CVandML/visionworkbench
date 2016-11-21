// __BEGIN_LICENSE__
//  Copyright (c) 2006-2013, United States Government as represented by the
//  Administrator of the National Aeronautics and Space Administration. All
//  rights reserved.
//
//  The NASA Vision Workbench is licensed under the Apache License,
//  Version 2.0 (the "License"); you may not use this file except in
//  compliance with the License. You may obtain a copy of the License at
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
// __END_LICENSE__


/// \file vwv_MainWidget.cc
///
/// The Vision Workbench image viewer.
///

#include <string>
#include <vector>

// Qt
#include <QtGui>

// Vision Workbench
#include <vw/Image.h>
#include <vw/FileIO.h>
#include <vw/Math/EulerAngles.h>
#include <vw/Image/Algorithms.h>
#include <vw/gui/MainWidget.h>
using namespace vw;
using namespace vw::gui;
using namespace std;

void popUp(std::string msg){
  QMessageBox msgBox;
  msgBox.setText(msg.c_str());
  msgBox.exec();
  return;
}

void do_hillshade(cartography::GeoReference const& georef,
                  ImageView<float> & img,
                  float nodata_val){

  // Copied from hillshade.cc.
  
  // Select the pixel scale
  float u_scale, v_scale;
  u_scale = georef.transform()(0,0);
  v_scale = georef.transform()(1,1);

  int elevation = 45;
  int azimuth   = 0;
  
  // Set the direction of the light source.
  Vector3f light_0(1,0,0);
  Vector3f light
    = vw::math::euler_to_rotation_matrix(elevation*M_PI/180,
                                         azimuth*M_PI/180, 0, "yzx") * light_0;
  

  ImageViewRef<PixelMask<float> > masked_img = create_mask(img, nodata_val);

  // The final result is the dot product of the light source with the normals
  ImageView<PixelMask<uint8> > shaded_image =
    channel_cast_rescale<uint8>
    (clamp
     (dot_prod
      (compute_normals
       (masked_img, u_scale, v_scale), light)));

  img = apply_mask(shaded_image);
}

void imageData::read(std::string const& image, bool ignore_georef,
                     bool hillshade){
  name = image;
  img = DiskImageView<float>(name);

  if (ignore_georef)
    has_georef = false;
  else
    has_georef = vw::cartography::read_georeference(georef, name);
    
  if (has_georef)
    bbox = georef.lonlat_bounding_box(img); // lonlat box
  else
    bbox = bounding_box(img);               // pixel box

  boost::shared_ptr<DiskImageResource> rsrc(DiskImageResource::open(name));
  nodata_val = -FLT_MAX;
  if ( rsrc->has_nodata_read() ) {
    nodata_val = rsrc->nodata_read();
  }
  
  // Find the min and max values in an image
  ChannelTypeEnum channel_type = rsrc->channel_type();
  if(channel_type == VW_CHANNEL_UINT8){

    min_val = 0;
    max_val = 255;
  }else{

    min_val = FLT_MAX;
    max_val = -FLT_MAX;
    for (int col = 0; col < img.cols(); col++){
      for (int row = 0; row < img.rows(); row++){
        if (img(col, row) <= nodata_val) continue;
        if (img(col, row) < min_val) min_val = img(col, row);
        if (img(col, row) > max_val) max_val = img(col, row);
      }
    }
    if (min_val >= max_val)
      max_val = min_val + 1.0;
  }

  if (hillshade){
    if (!has_georef){
      popUp("Cannot create hillshade if the image has no georeference.");
      exit(1);
    }
    do_hillshade(georef, img, nodata_val);

  }else{
  
    // Normalize to 0 - 255, taking into account the nodata_val
    for (int col = 0; col < img.cols(); col++){
      for (int row = 0; row < img.rows(); row++){
        img(col, row) = round(255*(std::max(double(img(col, row)), min_val)
                                   - min_val)/(max_val-min_val));
      }
    }
  }
}

vw::Vector2 vw::gui::QPoint2Vec(QPoint const& qpt) {
  return vw::Vector2(qpt.x(), qpt.y());
}
QPoint vw::gui::Vec2QPoint(vw::Vector2 const& V) {
  return QPoint(round(V.x()), round(V.y()));
}

// Allow the user to choose which files to hide/show in the GUI.
// User's choice will be processed by MainWidget::showFilesChosenByUser().
chooseFilesDlg::chooseFilesDlg(QWidget * parent):
  QWidget(parent){

  setWindowModality(Qt::ApplicationModal); 

  int spacing = 0;
  
  QVBoxLayout * vBoxLayout = new QVBoxLayout(this);
  vBoxLayout->setSpacing(spacing);
  vBoxLayout->setAlignment(Qt::AlignLeft);

  // The layout having the file names. It will be filled in
  // dynamically later.
  m_filesTable = new QTableWidget();

  m_filesTable->horizontalHeader()->hide();
  m_filesTable->verticalHeader()->hide();
    
  vBoxLayout->addWidget(m_filesTable);

  return;
}

chooseFilesDlg::~chooseFilesDlg(){}

void chooseFilesDlg::chooseFiles(const std::vector<imageData> & images){

  // See the top of this file for documentation.
  
  int numFiles = images.size();
  int numCols = 2;
  m_filesTable->setRowCount(numFiles);
  m_filesTable->setColumnCount(numCols);

  for (int fileIter = 0; fileIter < numFiles; fileIter++){

    QTableWidgetItem *item;    
    item = new QTableWidgetItem(1);
    item->data(Qt::CheckStateRole);
    item->setCheckState(Qt::Checked);
    m_filesTable->setItem(fileIter, 0, item);
    
    string fileName = images[fileIter].name;
    item = new QTableWidgetItem(fileName.c_str());
    item->setFlags(Qt::NoItemFlags);
    item->setForeground(QColor::fromRgb(0,0,0));
    m_filesTable->setItem(fileIter, numCols - 1, item);
    
  }
  
  QStringList rowNamesList;
  for (int fileIter = 0; fileIter < numFiles; fileIter++) rowNamesList << "";
  m_filesTable->setVerticalHeaderLabels(rowNamesList);

  QStringList colNamesList;
  for (int colIter = 0; colIter < numCols; colIter++) colNamesList << "";
  m_filesTable->setHorizontalHeaderLabels(colNamesList);
  QTableWidgetItem * hs = m_filesTable->horizontalHeaderItem(0);
  hs->setBackground(QBrush(QColor("lightgray")));
  
  m_filesTable->setSelectionMode(QTableWidget::ExtendedSelection);
  string style = string("QTableWidget::indicator:unchecked ")
    + "{background-color:white; border: 1px solid black;}; " +
    "selection-background-color: rgba(128, 128, 128, 40);";

  m_filesTable->setSelectionMode(QTableWidget::NoSelection);

  m_filesTable->setStyleSheet(style.c_str());
  m_filesTable->resizeColumnsToContents();
  m_filesTable->resizeRowsToContents();
  
  // The processing of user's choice happens in MainWidget::showFilesChosenByUser()
  
  return;
}


// --------------------------------------------------------------
//               MainWidget Public Methods
// --------------------------------------------------------------

MainWidget::MainWidget(QWidget *parent,
                       std::vector<std::string> const& images,
                       chooseFilesDlg * chooseFiles,
                       bool ignore_georef,
                       bool hillshade)
  : QWidget(parent), m_chooseFilesDlg(chooseFiles){

  installEventFilter(this);

  // Set default values
  m_nodata_value = 0;
  m_use_nodata = 0;
  m_image_min = 0;
  m_image_max = 1.0;

  // Set some reasonable defaults
  m_bilinear_filter = true;
  m_use_colormap = false;
  m_adjust_mode = NoAdjustment;
  m_display_channel = DisplayRGBA;
  m_colorize_display = false;
  m_hillshade_display = false;

  // Set up shader parameters
  m_gain = 1.0;
  m_offset = 0.0;
  m_gamma = 1.0;

  // Set mouse tracking
  this->setMouseTracking(true);

  // Set the size policy that the widget can grow or shrink and still
  // be useful.
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  this->setFocusPolicy(Qt::ClickFocus);

  int num_images = images.size();
  m_images.resize(num_images);
  for (int i = 0; i < num_images; i++){
    m_images[i].read(images[i], ignore_georef, hillshade);
    m_images_box.grow(m_images[i].bbox);
  }

  // To do: Warn the user if some images have georef
  // while others don't.
  
  // Choose which files to hide/show in the GUI
  if (m_chooseFilesDlg){
    QObject::connect(m_chooseFilesDlg->getFilesTable(),
                     SIGNAL(itemClicked(QTableWidgetItem *)),
                     this,
                     SLOT(showFilesChosenByUser())
                     );
    m_chooseFilesDlg->chooseFiles(m_images);
  }
  
}


MainWidget::~MainWidget() {
}

bool MainWidget::eventFilter(QObject *obj, QEvent *E){
  return QWidget::eventFilter(obj, E);
}

void MainWidget::showFilesChosenByUser(){
  
  // Process user's choice from m_chooseFilesDlg.

  if (!m_chooseFilesDlg)
    return;

  m_filesToHide.clear();
  QTableWidget * filesTable = m_chooseFilesDlg->getFilesTable();
  int rows = filesTable->rowCount();

  for (int rowIter = 0; rowIter < rows; rowIter++){
    QTableWidgetItem *item = filesTable->item(rowIter, 0);
    if (item->checkState() != Qt::Checked){
      string fileName
        = (filesTable->item(rowIter, 1)->data(0)).toString().toStdString();
      m_filesToHide.insert(fileName);
    }
  }
  
  update();
  
  return;
}


void MainWidget::size_to_fit() {
  double aspect = double(m_window_width) / m_window_height;
  double maxdim = std::max(m_images_box.width(),m_images_box.height());
  if (m_images_box.width() > m_images_box.height()) {
    double width = maxdim;
    double height = maxdim/aspect;
    double extra = height - m_images_box.height();
    m_current_view = BBox2(Vector2(0.0, -extra/2),
                           Vector2(width, height-extra/2));
  } else {
    double width = maxdim*aspect;
    double height = maxdim;
    double extra = width - m_images_box.width();
    m_current_view = BBox2(Vector2(-extra/2, 0.0),
                           Vector2(width-extra/2, height));
  }

  // So far we just found the width and height of the current view.
  // Now place it in the right location.
  m_current_view += m_images_box.min();
  
  update();
}

void MainWidget::zoom(double scale) {

  updateCurrentMousePosition();
  scale = std::max(1e-8, scale);
  BBox2 current_view = (m_current_view - m_curr_world_pos) / scale
    + m_curr_world_pos;

  if (!current_view.empty()){
  // Check to make sure we haven't hit our zoom limits...
    m_current_view = current_view;
    update(); // will call paintEvent()
  }
}

void MainWidget::resizeEvent(QResizeEvent*){
  QRect v       = this->geometry();
  m_window_width = v.width();
  m_window_height = v.height();
  size_to_fit();
  return;
}


// --------------------------------------------------------------
//             MainWidget Private Methods
// --------------------------------------------------------------

void MainWidget::drawImage(QPainter* paint) {

  // The portion of the image to draw
  for (int i = 0; i < (int)m_images.size(); i++){

    // Don't show files the user wants hidden
    string fileName = m_images[i].name;
    if (m_filesToHide.find(fileName) != m_filesToHide.end()) continue;
    
    BBox2 image_box = m_current_view;
    image_box.crop(m_images[i].bbox);
    
    // See where it fits on the screen
    BBox2i pixel_box;
    pixel_box.grow(round(world2pixel(image_box.min())));
    pixel_box.grow(round(world2pixel(image_box.max())));
    
    QImage qimg;
    if (m_images[i].has_georef){
      // image_box is in lonlat domain. Go from screen pixels
      // to lonlat, then to image pixels. We need to do a flip in
      // y since in lonlat space the origin is in lower-left corner.
      
      ImageView<float> & img = m_images[i].img;
      qimg = QImage(pixel_box.width(), pixel_box.height(), QImage::Format_RGB888);
      int len = pixel_box.max().y() - pixel_box.min().y() - 1;
      for (int x = pixel_box.min().x(); x < pixel_box.max().x(); x++){
        for (int y = pixel_box.min().y(); y < pixel_box.max().y(); y++){
          Vector2 lonlat = pixel2world(Vector2(x, y));
          Vector2 p = round(m_images[i].georef.lonlat_to_pixel(lonlat));
          if (p[0] >= 0 && p[0] < img.cols() &&
              p[1] >= 0 && p[1] < img.rows() ){
            float v = img(p[0], p[1]); // is it better to interp?
            qimg.setPixel(x-pixel_box.min().x(),
                          len - (y-pixel_box.min().y()), // flip pixels in y
                          qRgb(v, v, v));
          }
        }
      }

      // flip pixel box in y
      QRect v = this->geometry();
      int a = pixel_box.min().y() - v.y();
      int b = v.y() + v.height() - pixel_box.max().y();
      pixel_box.min().y() = pixel_box.min().y() + b - a;
      pixel_box.max().y() = pixel_box.max().y() + b - a;

      QRect rect(pixel_box.min().x(),
                 pixel_box.min().y(),
                 pixel_box.width(), pixel_box.height());
      paint->drawImage (rect, qimg);
      
    }else{
      // image_box is in image pixel domain.
      ImageView<float> img = crop(m_images[i].img, image_box);
      qimg = QImage(img.cols(), img.rows(), QImage::Format_RGB888);
      for (int x = 0; x < img.cols(); x++) {
        for (int y = 0; y < img.rows(); y++) {
          qimg.setPixel(x, y, qRgb(img(x, y), img(x, y), img(x, y)));
        }
      }
    
      QRect rect(pixel_box.min().x(), pixel_box.min().y(),
                 pixel_box.width(), pixel_box.height());
      paint->drawImage (rect, qimg);
    }
  }
  
  return;
}

vw::Vector2 MainWidget::world2pixel(vw::Vector2 const& p){
  // Convert a position in the world coordinate system to a pixel value,
  // relative to the image seen on screen (the origin is an image corner).
  double x = m_window_width*((p.x() - m_current_view.min().x())
                             /m_current_view.width());
  double y = m_window_height*((p.y() - m_current_view.min().y())
                              /m_current_view.height());
  return vw::Vector2(x, y);
}

vw::Vector2 MainWidget::pixel2world(vw::Vector2 const& pix){
  // Convert a pixel on the screen (the origin is an image corner),
  // to global world coordinates.
  double x = m_current_view.min().x()
    + m_current_view.width() * double(pix.x()) / m_window_width;
  double y = m_current_view.min().y()
    + m_current_view.height() * double(pix.y()) / m_window_height;
  return vw::Vector2(x, y);
}

void MainWidget::updateCurrentMousePosition() {
  m_curr_world_pos = pixel2world(m_curr_pixel_pos);
}

// --------------------------------------------------------------
//             MainWidget Event Handlers
// --------------------------------------------------------------

void MainWidget::paintEvent(QPaintEvent * /* event */) {
  QPainter paint(this);
  drawImage(&paint);
}

void MainWidget::mousePressEvent(QMouseEvent *event) {
  m_mouse_press_pos = event->pos();

  m_curr_pixel_pos = QPoint2Vec(m_mouse_press_pos);
  m_last_gain = m_gain;     // Store this so the user can do linear
  m_last_offset = m_offset; // and nonlinear steps.
  m_last_gamma = m_gamma;
  m_last_viewport_min = QPoint( m_current_view.min().x(),
                                m_current_view.min().y() );
  updateCurrentMousePosition();
}

void MainWidget::mouseReleaseEvent ( QMouseEvent *event ){

  QPoint mouse_rel_pos = event->pos();

  int tol = 5;
  if (std::abs(mouse_rel_pos.x() - m_mouse_press_pos.x()) < tol &&
      std::abs(mouse_rel_pos.y() - m_mouse_press_pos.y()) < tol
      ){
    // If the mouse was released too close to where it was clicked,
    // do nothing.
    return;
  }

  // Drag the image along the mouse movement
  m_current_view -= (pixel2world(QPoint2Vec(event->pos())) -
                     pixel2world(QPoint2Vec(m_mouse_press_pos)));

  update(); // will call paintEvent()
  
  return;
}

void MainWidget::mouseMoveEvent(QMouseEvent *event) {
  // Diff variables are just the movement of the mouse normalized to
  // 0.0-1.0;
  double x_diff = double(event->x() - m_curr_pixel_pos.x()) / m_window_width;
  double y_diff = double(event->y() - m_curr_pixel_pos.y()) / m_window_height;
  double width = m_current_view.width();
  double height = m_current_view.height();

  // Right mouse button just kicks up the gain of all mouse actions
  if (event->buttons() & Qt::LeftButton || event->buttons() & Qt::RightButton) {

    if ( event->buttons() & Qt::RightButton ) {
      x_diff *= 2;
      y_diff *= 2;
    }

    std::ostringstream s;
    switch (m_adjust_mode) {

    case NoAdjustment:
      break;
    case TransformAdjustment:
      m_current_view.min().x() =
        m_last_viewport_min.x() - x_diff * width;
      m_current_view.min().y() =
        m_last_viewport_min.y() - y_diff * height;
      m_current_view.max().x() =
        m_last_viewport_min.x() - x_diff * width + width;
      m_current_view.max().y() =
        m_last_viewport_min.y() - y_diff * height + height;
      break;

    case GainAdjustment:
      m_gain = m_last_gain * pow(2.0,x_diff);
      s << "Gain: " << (log(m_gain)/log(2)) << " f-stops\n";
      break;

    case OffsetAdjustment:
      m_offset = m_last_offset +
        (pow(100,fabs(x_diff))-1.0)*(x_diff > 0 ? 0.1 : -0.1);
      s << "Offset: " << m_offset << "\n";
      break;

    case GammaAdjustment:
      m_gamma = m_last_gamma * pow(2.0,x_diff);
      s << "Gamma: " << m_gamma << "\n";
      break;
    }
  }

  updateCurrentMousePosition();
}

void MainWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  m_curr_pixel_pos = QPoint2Vec(event->pos());
  updateCurrentMousePosition();
}

void MainWidget::wheelEvent(QWheelEvent *event) {
  int num_degrees = event->delta();
  double num_ticks = double(num_degrees) / 360;

  // 2.0 chosen arbitrarily here as a reasonable scale factor giving good
  // sensitivity of the mousewheel. Shift zooms 50 times slower.
  double scale_factor = 2;
  if (event->modifiers() & Qt::ShiftModifier)
    scale_factor *= 50;

  double mag = fabs(num_ticks/scale_factor);
  double scale = 1;
  if (num_ticks > 0)
    scale = 1+mag;
  else if (num_ticks < 0)
    scale = 1-mag;

  zoom(scale);

  m_curr_pixel_pos = QPoint2Vec(event->pos());
  updateCurrentMousePosition();
}


void MainWidget::enterEvent(QEvent */*event*/) {
}

void MainWidget::leaveEvent(QEvent */*event*/) {
}

void MainWidget::keyPressEvent(QKeyEvent *event) {

  std::ostringstream s;

  switch (event->key()) {
  case Qt::Key_I:  // Toggle bilinear/nearest neighbor interp
    m_bilinear_filter = !m_bilinear_filter;
    break;
  case Qt::Key_C:  // Activate colormap
    m_use_colormap = !m_use_colormap;
    break;
  case Qt::Key_H:  // Activate hillshade
    if ( m_hillshade_display == 0 ) {
      m_hillshade_display = 1;
    } else {
      m_hillshade_display *= 3;
    }
    if ( m_hillshade_display > 100 || m_hillshade_display < 0 )
      m_hillshade_display = 0;
    break;
  case Qt::Key_G:  // Gain adjustment mode
    if (m_adjust_mode == GainAdjustment) {
      m_adjust_mode = TransformAdjustment;
    } else {
      m_adjust_mode = GainAdjustment;
      s << "Gain: " << log(m_gain)/log(2) << " f-stops\n";
    }
    break;
  case Qt::Key_O:  // Offset adjustment mode
    if (m_adjust_mode == OffsetAdjustment) {
      m_adjust_mode = TransformAdjustment;
    } else {
      m_adjust_mode = OffsetAdjustment;
      s << "Offset: " << m_offset;
    }
    break;
  case Qt::Key_V:  // Gamma adjustment mode
    if (m_adjust_mode == GammaAdjustment) {
      m_adjust_mode = TransformAdjustment;
    } else {
      m_adjust_mode = GammaAdjustment;
      s << "Gamma: " << m_gamma;
    }
    break;
  case Qt::Key_1:  // Display red channel only
    m_display_channel = DisplayR;
    break;
  case Qt::Key_2:  // Display green channel only
    m_display_channel = DisplayG;
    break;
  case Qt::Key_3:  // Display blue channel only
    m_display_channel = DisplayB;
    break;
  case Qt::Key_4:  // Display alpha channel only
    m_display_channel = DisplayA;
    break;
  case Qt::Key_0:  // Display all color channels
    m_display_channel = DisplayRGBA;
    break;
  default:
    QWidget::keyPressEvent(event);
  }
}


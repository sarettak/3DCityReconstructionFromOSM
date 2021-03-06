# 3D City Reconstruction From OpenStreetMap Data
The developed application is an open-source tool, based on [Yocto/GL](https://github.com/xelatihy/yocto-gl) library, suitable to reconstruct cities at scale from OpenStreetMap data in hundreds of seconds. Additionally, [Earcut](https://github.com/mapbox/earcut.hpp) (for triangulation) and [JSON](https://github.com/nlohmann/json) (for parsing tags from GeoJSON files) are employed in the system. 

## Prerequisites
The application requires a C++17 compiler:
* OsX (Xcode >= 11)
* Windows (MSVC 2019)
* Linux (gcc >= 9, clang >= 9)


## Getting started

### Installation
* Clone this repository:

```
git clone https://github.com/sarettak/3DCityReconstructionFromOSM
cd 3DCityReconstructionFromOSM
```

### Compilation
Compile the code:

```
cd scripts/build.sh
```

## Creating the city
Launch the application: 
```
./bin/ycityproc ./city/cities_geojson/name -o ./tests/name/name.json
```

in which:
* `./city/cities_geojson/name` specifies the path to the GeoJSON file(s) of the desired city to reconstruct (identified by `name`)
* `./tests/name/name.json` defines the path for saving the scene in a format understandable by Yocto/GL library

#### Example
```
./bin/ycityproc ./city/cities_geojson/berlin -o ./tests/berlin/berlin.json
```

## Visualizing the city
The view of the reconstructed 3D city reconstructed is displayed in the GUI and saved as a .png image by running the following command:
```
./bin/ysceneitraces tests/name/name.json -o name.png   
```
in which:
* `./tests/name/name.json` specifies the path to the scene of the city (identified by `name`) in a format understandable by Yocto/GL library
* `name.png` assigns the name `name` to the image representing the generated city

#### Example
```
./bin/ysceneitraces tests/berlin/berlin.json -o berlin.png  
```

## Reconstructed cities
Currently, the following cities can be directly visualized in the GUI without creating them from scratch:
* Amsterdam
* London
* Paris
* Rome 
* Tokyo

However, other cities of interest can be reconstructed by downloading the GeoJSON data information of the desired area through [Overpass Turbo](https://overpass-turbo.eu) and positioning the file(s) in a dedicated folder at the path `city/cities_geojson/`

#### Some results
Amsterdam  <br />
<img src="images/amsterdam.jpg" width="458" height="457">

Berlin  <br />
<img src="images/berlin.jpg" width="460" height="461">

London  <br />
<img src="images/london.jpg" width="459" height="459">

Rome  <br />
<img src="images/rome_all_morning.jpg" width="455" height="381">

## Citation
If you are using this code in your project or research, please cite:

```
@inproceedings {10.2312:stag.20211474,
booktitle = {Smart Tools and Apps for Graphics - Eurographics Italian Chapter Conference},
editor = {Frosini, Patrizio and Giorgi, Daniela and Melzi, Simone and Rodol??, Emanuele},
title = {{3D City Reconstruction from OpenStreetMap Data}},
author = {Kaszuba, Sara and Pellacini, Fabio},
year = {2021},
publisher = {The Eurographics Association},
ISSN = {2617-4855},
ISBN = {978-3-03868-165-6},
DOI = {10.2312/stag.20211474}
}
```

## Authors
Sara Kaszuba & Fabio Pellacini



bool helpOpen=false;
int helpTab=0;
constexpr std::array helpTabs{"Quick start","Controls","Ecosystem","Cells & colors"};

void drawCellLegend(life::skia::Api::Canvas canvas,life::skia::Api::Paint paint,float width){
  using life::CellType;
  using life::TerrainType;
  using life::ResourceType;
  struct Entry {uint32_t color;const char* name;const char* description;};
  constexpr Entry cells[]={
    {cellColors[int(CellType::Mouth)],"Mouth","Eats nearby plant matter."},
    {cellColors[int(CellType::PlantMouth)],"Plant mouth","Eats nearby plant matter."},
    {cellColors[int(CellType::ScavengerMouth)],"Scavenger mouth","Eats carrion for energy."},
    {cellColors[int(CellType::MineralMouth)],"Mineral mouth","Collects minerals to build cells."},
    {cellColors[int(CellType::DecomposerMouth)],"Decomposer mouth","Eats carrion and returns nutrients to soil."},
    {cellColors[int(CellType::Producer)],"Producer","Grows plant matter using energy and soil nutrients."},
    {cellColors[int(CellType::Mover)],"Mover","Lets the organism move using its neural network."},
    {cellColors[int(CellType::Killer)],"Killer","Attacks adjacent creatures and wears out with use."},
    {cellColors[int(CellType::Armor)],"Armor","Protects the cell from attacks."},
    {cellColors[int(CellType::Inert)],"Inert tissue","Spent tissue with no active role."},
    {cellColors[int(CellType::Egg)],"Egg","Develops into a juvenile."},
    {cellColors[int(CellType::Food)],"Food cell","Legacy plant food."},
    {cellColors[int(CellType::Wall)],"Wall cell","Legacy obstacle."}
  };
  constexpr Entry world[]={
    {terrainColors[int(TerrainType::Plains)],"Plains",nullptr},
    {terrainColors[int(TerrainType::Fertile)],"Fertile soil",nullptr},
    {terrainColors[int(TerrainType::Desert)],"Desert",nullptr},
    {terrainColors[int(TerrainType::Water)],"Water (blocks movement)",nullptr},
    {terrainColors[int(TerrainType::Mountain)],"Mountain (blocks movement)",nullptr},
    {resourceColors[int(ResourceType::Plant)],"Plant matter",nullptr},
    {resourceColors[int(ResourceType::Carrion)],"Carrion",nullptr},
    {resourceColors[int(ResourceType::Mineral)],"Minerals",nullptr}
  };
  const float column=(width-48)/2;
  const auto drawEntry=[&](const Entry& entry,size_t index,float top,float spacing){
    const float x=24+(index%2)*column,y=top+(index/2)*spacing;
    rectangle(canvas,paint,x,y,x+15,y+15,entry.color);
    boundedText(canvas,paint,entry.name,x+23,y+11,11,0xffffffff,x+column-8,true);
    if(entry.description)
      boundedText(canvas,paint,entry.description,x+23,y+25,10,0xffb8c3d1,x+column-8);
  };
  boundedText(canvas,paint,"Organism cells and legacy tiles",24,211,12,0xff81d2c7,width-24,true);
  for(size_t i=0;i<std::size(cells);++i)drawEntry(cells[i],i,221,33);
  boundedText(canvas,paint,"Terrain and resource overlays",24,461,12,0xff81d2c7,width-24,true);
  for(size_t i=0;i<std::size(world);++i)drawEntry(world[i],i,471,22);
  boundedText(canvas,paint,"Species, Disease, and Development overlays can recolor cells.",24,580,10,0xffb8c3d1,width-24);
}

void toggleHelp(){
  helpOpen=!helpOpen;
  if(helpOpen){atlasOpen=false;researchOpen=false;dragging=false;}
}

void drawHelp(life::skia::Api::Canvas canvas,life::skia::Api::Paint paint,float width,float height){
  rectangle(canvas,paint,0,82,width,height-28,0xff121d29);
  boundedText(canvas,paint,"HELP",24,120,24,0xffffffff,width-24,true);
  boundedText(canvas,paint,"Start a world and see how its creatures adapt.",24,143,12,0xffb8c3d1,width-24);
  const float tabWidth=(width-48)/helpTabs.size();
  for(int i=0;i<int(helpTabs.size());++i){
    const float left=24+i*tabWidth;
    uiButton(canvas,paint,left,158,left+tabWidth-5,190,i==helpTab?0xff81d2c7:0xff3a4b68);
    boundedText(canvas,paint,helpTabs[i],left+10,179,12,i==helpTab?0xff121d29:0xffffffff,left+tabWidth-10,true);
  }
  struct Row {const char* title;const char* detail;};
  constexpr Row pages[][6]={
    {{"1. Watch the world","Let the simulation run. Organisms feed, reproduce, mutate, and die."},
     {"2. Inspect a creature","Select Inspect under World tools, then click a creature to see its details."},
     {"3. Follow a lineage","Press F to follow the selected creature. Press G to toggle family highlights."},
     {"4. Try an intervention","Paint plant matter or nutrients and watch how the population responds."},
     {"5. Explore the history","Open Atlas with A for population and species. Open Research with R for experiments."},
     {"6. Save your world","Press F5 to save your progress. Press F9 to load it again."}},
    {{"Move around","Scroll to zoom. Drag with the middle mouse button, or select Pan and drag."},
     {"Pause and speed","Pause stops simulation. TPS sets simulation speed. FPS sets the render cap."},
     {"Creature view","F follows, G toggles family, C toggles compact mode. Use [ and ] to browse decisions."},
     {"Follow descendants","After a followed creature dies, press D to follow a surviving descendant."},
     {"Save and load","F5 saves and F9 loads. Hold Shift with either key to choose an archive."},
     {"Open a page","A opens Atlas. R opens Research. F1 or H opens Help. Esc closes Help."}},
    {{"Food and soil","Plant matter feeds grazers. Carrion feeds scavengers and decomposers."},
     {"Minerals and producers","Minerals help build cells. Producer Cells turn energy and nutrients into plant matter."},
     {"Moving producers","Enable the Simulation checkbox to let movers produce food. It starts off."},
     {"Terrain and seasons","Water and mountains block movement. Seasons change temperature and fertility."},
     {"Evolution","Offspring inherit traits with mutations. Moving creatures use NNUE to choose actions."},
     {"Overlays","Click Overlay to cycle terrain, resources, species, chemical signals, disease, and more."}}
  };
  float y=220;
  if(helpTab==int(helpTabs.size())-1)drawCellLegend(canvas,paint,width);
  else for(const auto& row:pages[helpTab]){
    if(y+22>height-52)break;
    boundedText(canvas,paint,row.title,30,y,13,0xff81d2c7,width-24,true);
    boundedText(canvas,paint,row.detail,30,y+20,12,0xffffffff,width-24);
    y+=53;
  }
  boundedText(canvas,paint,"F1 / H / Esc closes Help",24,height-39,10,0xffb8c3d1,width-24);
}

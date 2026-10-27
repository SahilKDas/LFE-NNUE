struct CachedText {life::skia::Api::TextBlob blob{};uint64_t used{};};
std::unordered_map<std::string,CachedText> textBlobs;
std::unordered_map<uint64_t,life::skia::Api::Font> textFonts;
uint64_t textClock=0,textBuilds=0;bool textCacheEnabled=true;

void releaseTextCache(){
  for(const auto& [key,entry]:textBlobs)if(entry.blob)api->textBlobUnref(entry.blob);
  for(const auto& [key,font]:textFonts)if(font)api->fontDelete(font);
  textBlobs.clear();textFonts.clear();
}
life::skia::Api::TextBlob buildTextBlob(life::skia::Api::Font font,const std::string& value){
  const int count=api->fontTextToGlyphs(font,value.data(),value.size(),0,nullptr,0);if(count<=0)return nullptr;
  std::vector<uint16_t> glyphs(count);api->fontTextToGlyphs(font,value.data(),value.size(),0,glyphs.data(),count);auto builder=api->textBlobBuilderNew();life::skia::RunBuffer run{};api->textBlobBuilderAllocRun(builder,font,count,0,0,nullptr,&run);std::memcpy(run.glyphs,glyphs.data(),glyphs.size()*sizeof(uint16_t));auto blob=api->textBlobBuilderMake(builder);api->textBlobBuilderDelete(builder);++textBuilds;return blob;
}
void text(life::skia::Api::Canvas canvas,life::skia::Api::Paint paint,const std::string& value,float x,float y,float size,uint32_t color,bool bold=false){
  if(value.empty())return;life::skia::Api::TextBlob blob{};bool temporary=false;
  if(!textCacheEnabled){auto font=api->fontNew(bold?boldTypeface:regularTypeface,size,1,0);blob=buildTextBlob(font,value);api->fontDelete(font);temporary=true;}
  else{
    std::string key=value;key.append(reinterpret_cast<const char*>(&size),sizeof(size));key.push_back(bold?1:0);
    auto found=textBlobs.find(key);if(found==textBlobs.end()){
      if(textBlobs.size()>=512){auto oldest=textBlobs.begin();for(auto entry=textBlobs.begin();entry!=textBlobs.end();++entry)if(entry->second.used<oldest->second.used)oldest=entry;api->textBlobUnref(oldest->second.blob);textBlobs.erase(oldest);}
      uint32_t bits{};std::memcpy(&bits,&size,sizeof(bits));const uint64_t fontKey=(uint64_t(bits)<<1)|uint64_t(bold);auto cached=textFonts.find(fontKey);if(cached==textFonts.end())cached=textFonts.emplace(fontKey,api->fontNew(bold?boldTypeface:regularTypeface,size,1,0)).first;
      blob=buildTextBlob(cached->second,value);if(!blob)return;found=textBlobs.emplace(std::move(key),CachedText{blob,0}).first;
    }
    found->second.used=++textClock;blob=found->second.blob;
  }
  if(blob){api->paintSetColor(paint,color);api->paintSetAntialias(paint,true);api->canvasDrawTextBlob(canvas,blob,x,y,paint);if(temporary)api->textBlobUnref(blob);}
}

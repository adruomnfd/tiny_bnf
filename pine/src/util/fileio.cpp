#include <core/scene.h>
#include <util/profiler.h>
#include <util/archive.h>
#include <util/huffman.h>
#include <util/fileio.h>
#include <util/parser.h>
#include <util/misc.h>
#include <util/log.h>

namespace pine {

pstd::string sceneDirectory = "";

ScopedFile::ScopedFile(pstd::string_view filename_view, pstd::ios::openmode mode) {
    auto filename = (pstd::string)filename_view;
    filename = sceneDirectory + filename;
    pstd::replace(filename.begin(), filename.end(), '\\', '/');
    CHECK(filename != "");
    file.open(filename.c_str(), mode);
    if (file.is_open() == false)
        LOG_WARNING("[ScopedFile]Can not open file \"&\"", filename.c_str());
}
void ScopedFile::Write(const void *data, size_t size) {
    if (file.is_open())
        file.write((char *)data, size);
}
void ScopedFile::Read(void *data, size_t size) {
    if (file.is_open())
        file.read((char *)data, size);
}

bool IsFileExist(pstd::string_view filename_view) {
    auto filename = (pstd::string)filename_view;
    pstd::replace(filename.begin(), filename.end(), '\\', '/');
    pstd::fstream file(filename, pstd::ios::in);
    return file.is_open();
}
pstd::string GetFileDirectory(pstd::string_view filename_view) {
    auto filename = (pstd::string)filename_view;
    pstd::replace(filename.begin(), filename.end(), '\\', '/');

    size_t forwardslash = pstd::find_last_of(begin(filename), end(filename), '/') - begin(filename);
    if (pstd::find_last_of(begin(filename), end(filename), '/') != end(filename))
        filename = trim(filename, 0, forwardslash);

    return (pstd::string)filename + "/";
}
pstd::string GetFileExtension(pstd::string_view filename) {
    size_t p = pstd::find_last_of(begin(filename), end(filename), '.') - begin(filename);
    if (pstd::find_last_of(begin(filename), end(filename), '.') == end(filename))
        return "";

    return (pstd::string)trim(filename, p + 1, filename.size() - p - 1);
}
pstd::string RemoveFileExtension(pstd::string_view filename) {
    size_t p = pstd::find(begin(filename), end(filename), '.') - begin(filename);
    if (pstd::find(begin(filename), end(filename), '.') == end(filename))
        return "";

    return (pstd::string)trim(filename, 0, p);
}
pstd::string ChangeFileExtension(pstd::string_view filename, pstd::string ext) {
    return RemoveFileExtension(filename) + "." + ext;
}
pstd::string AppendFileName(pstd::string_view filename, pstd::string content) {
    return RemoveFileExtension(filename) + content + "." + GetFileExtension(filename);
}

pstd::string ReadStringFile(pstd::string_view filename) {
    ScopedFile file(filename, pstd::ios::in | pstd::ios::binary);
    size_t size = file.Size();
    pstd::string str;
    str.resize(size);
    file.Read(&str[0], size);
    return str;
}
void WriteBinaryData(pstd::string_view filename, const void *ptr, size_t size) {
    ScopedFile file(filename, pstd::ios::binary | pstd::ios::out);
    file.Write((const char *)ptr, size);
}
pstd::vector<char> ReadBinaryData(pstd::string_view filename) {
    ScopedFile file(filename, pstd::ios::binary | pstd::ios::in);
    pstd::vector<char> data(file.Size());
    file.Read(&data[0], file.Size());
    return data;
}

void WriteImageBMP(pstd::string_view filename, vec2i size, int nchannel, const uint8_t *data) {
    ScopedFile file(filename, pstd::ios::out);

    pstd::vector<vec3u8> colors(size.x * size.y);
    for (int x = 0; x < size.x; x++)
        for (int y = 0; y < size.y; y++) {
            vec3u8 c;
            c.z = data[(x + y * size.x) * nchannel + 0];
            c.y = data[(x + y * size.x) * nchannel + 1];
            c.x = data[(x + y * size.x) * nchannel + 2];
            colors[x + (size.y - 1 - y) * size.x] = c;
        }

    int filesize = 54 + 3 * size.x * size.y;
    uint8_t header[] = {'B',
                        'M',
                        (uint8_t)(filesize),
                        (uint8_t)(filesize >> 8),
                        (uint8_t)(filesize >> 16),
                        (uint8_t)(filesize >> 24),
                        0,  // bfReserved1
                        0,
                        0,  // bfReserved2
                        0,
                        54,  // bfOffBits
                        0,
                        0,
                        0,
                        40,  // biSize
                        0,
                        0,
                        0,
                        (uint8_t)(size.x),
                        (uint8_t)(size.x >> 8),
                        (uint8_t)(size.x >> 16),
                        (uint8_t)(size.x >> 24),
                        (uint8_t)(size.y),
                        (uint8_t)(size.y >> 8),
                        (uint8_t)(size.y >> 16),
                        (uint8_t)(size.y >> 24),
                        1,  // biPlanes
                        0,
                        24,  // biBitCount
                        0,
                        0,  // biCompression
                        0,
                        0,
                        0,
                        0,  // biSizeImage
                        0,
                        0,
                        0,
                        0,  // biXPelsPerMeter
                        0,
                        0,
                        0,
                        0,  // biYPelsPerMeter
                        0,
                        0,
                        0,
                        0,  // biClrUsed
                        0,
                        0,
                        0,
                        0,  // biClrImportance
                        0,
                        0,
                        0};
    file.Write(header, sizeof(header));

    uint8_t padding[3] = {0, 0, 0};
    size_t paddingSize = (4 - (size.x * 3) % 4) % 4;
    if (paddingSize == 0) {
        file.Write(colors.data(), sizeof(colors[0]) * colors.size());
    } else {
        for (int y = 0; y < size.y; y++) {
            file.Write(colors.data() + y * size.x * 3, size.x * 3);
            file.Write(padding, paddingSize);
        }
    }
}
void SaveImage(pstd::string_view filename, vec2i size, int nchannel, const float *data) {
    pstd::vector<uint8_t> pixels(Area(size) * nchannel);
    for (int x = 0; x < size.x; x++)
        for (int y = 0; y < size.y; y++)
            for (int c = 0; c < nchannel; c++)
                pixels[y * size.x * nchannel + x * nchannel + c] = pstd::clamp(
                    data[y * size.x * nchannel + x * nchannel + c] * 256.0f, 0.0f, 255.0f);
    SaveImage(filename, size, nchannel, pixels.data());
}
void SaveImage(pstd::string_view filename, vec2i size, int nchannel, const uint8_t *data) {
    SWITCH(GetFileExtension(filename)) {
        CASE("bmp") WriteImageBMP(filename, size, nchannel, data);
        DEFAULT
        LOG_WARNING("& has unsupported image file extension", filename);
    }
}
vec3u8 *ReadLDRImage(pstd::string_view filename, vec2i & /*size*/) {
    //int nchannel = 0;
    uint8_t *data = nullptr;
    LOG_WARNING("[FileIO]Sorry, but image loading is not yet supported");
    if (!data)
        LOG_WARNING("[FileIO]Failed to load \"&\"", filename);
    return (vec3u8 *)data;
}

pstd::pair<pstd::vector<float>, vec3i> LoadVolume(pstd::string_view filename) {
    if (GetFileExtension(filename) == "compressed")
        return LoadCompressedVolume(filename);
    ScopedFile file(filename, pstd::ios::in | pstd::ios::binary);
    vec3i size = file.Read<vec3i>();
    pstd::vector<float> density(Volume(size));
    file.Read(&density[0], density.size() * sizeof(density[0]));

    return {pstd::move(density), size};
}
void CompressVolume(pstd::string /*filename*/, const pstd::vector<float> & /*densityf*/,
                    vec3i /*size*/) {
    // ScopedFile file(filename, pstd::ios::out | pstd::ios::binary);

    // for (size_t i = 0; i < densityf.size(); i++)
    //     if (densityf[i] > 32) {
    //         LOG_WARNING("Original density has voxel with value larger than 32, which will be "
    //                     "clamped to 32 for compression");
    //         break;
    //     }

    // pstd::vector<uint16_t> densityi(densityf.size());
    // for (size_t i = 0; i < densityf.size(); i++)
    //     densityi[i] =
    //         pstd::min(densityf[i], 32.0f) * int(pstd::numeric_limits<uint16_t>::max()) / 32;

    // auto tree = BuildHuffmanTree(densityi);
    // auto encoded = HuffmanEncode(tree, densityi);
    // auto treeData = Archive(tree);
    // auto encodedData = Archive(encoded);
    // file.Write(size);
    // file.Write(treeData.size());
    // file.Write(encodedData.size());
    // file.Write(treeData.data(), treeData.size() * sizeof(treeData[0]));
    // file.Write(encodedData.data(), encodedData.size() * sizeof(encodedData[0]));
}
pstd::pair<pstd::vector<float>, vec3i> LoadCompressedVolume(pstd::string_view /*filename*/) {
    // ScopedFile file(filename, pstd::ios::in | pstd::ios::binary);
    // vec3i size = file.Read<vec3i>();
    // size_t treeSize = file.Read<size_t>();
    // size_t encodedSize = file.Read<size_t>();
    // ArchiveBufferType treeData(treeSize);
    // ArchiveBufferType encodedData(encodedSize);
    // file.Read(&treeData[0], treeSize * sizeof(treeData[0]));
    // file.Read(&encodedData[0], encodedSize * sizeof(encodedData[0]));
    // auto tree = Unarchive<HuffmanTree<uint16_t>>(treeData);
    // auto encoded = Unarchive<HuffmanEncoded>(encodedData);
    // auto densityi = HuffmanDecode<pstd::vector<uint16_t>>(tree, encoded);

    // pstd::vector<float> densityf(densityi.size());
    // for (size_t i = 0; i < densityi.size(); i++)
    //     densityf[i] = densityi[i] / float(int(pstd::numeric_limits<uint16_t>::max()) / 32);

    // return {densityf, size};
    return {};
}

Parameters LoadScene(pstd::string_view filename, Scene *scene) {
    LOG("[FileIO]Loading \"&\"", filename);

    Parameters params = Parse(ReadStringFile(filename));

    sceneDirectory = GetFileDirectory(filename);

    Timer timer;

    for (auto &p : params.GetAll("Material"))
        scene->materials[p.GetString("name")] = pstd::make_shared<Material>(CreateMaterial(p));
    for (auto &p : params.GetAll("Medium"))
        scene->mediums[p.GetString("name")] = pstd::make_shared<Medium>(CreateMedium(p));
    for (auto &p : params.GetAll("Light"))
        scene->lights.push_back(CreateLight(p));
    for (auto &p : params.GetAll("Shape"))
        scene->shapes.push_back(CreateShape(p, scene));

    for (auto &shape : scene->shapes)
        if (auto light = shape.GetLight())
            scene->lights.push_back(*light);
    for (const Light &light : scene->lights)
        if (light.Is<EnvironmentLight>())
            scene->envLight = light.Be<EnvironmentLight>();

    scene->camera = CreateCamera(params["Camera"], scene);

    scene->integrator = pstd::shared_ptr<Integrator>(CreateIntegrator(params["Integrator"], scene));

    return params;
}

}  // namespace pine

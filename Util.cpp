#define _USE_MATH_DEFINES

#include "Util.h";
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "stb_image.h"
#include <cmath>

#define _CRT_SECURE_NO_WARNINGS
#include <fstream>
#include <sstream>
#include <iostream>

// Autor: Nedeljko Tesanovic
// Opis: pomocne funkcije za ucitavanje sejdera
unsigned int compileShader(GLenum type, const char* source)
{
    //Uzima kod u fajlu na putanji "source", kompajlira ga i vraca sejder tipa "type"
    //Citanje izvornog koda iz fajla
    std::string content = "";
    std::ifstream file(source);
    std::stringstream ss;
    if (file.is_open())
    {
        ss << file.rdbuf();
        file.close();
        std::cout << "Uspjesno procitao fajl sa putanje \"" << source << "\"!" << std::endl;
    }
    else {
        ss << "";
        std::cout << "Greska pri citanju fajla sa putanje \"" << source << "\"!" << std::endl;
    }
    std::string temp = ss.str();
    const char* sourceCode = temp.c_str(); //Izvorni kod sejdera koji citamo iz fajla na putanji "source"

    int shader = glCreateShader(type); //Napravimo prazan sejder odredjenog tipa (vertex ili fragment)

    int success; //Da li je kompajliranje bilo uspjesno (1 - da)
    char infoLog[512]; //Poruka o gresci (Objasnjava sta je puklo unutar sejdera)
    glShaderSource(shader, 1, &sourceCode, NULL); //Postavi izvorni kod sejdera
    glCompileShader(shader); //Kompajliraj sejder

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success); //Provjeri da li je sejder uspjesno kompajliran
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog); //Pribavi poruku o gresci
        if (type == GL_VERTEX_SHADER)
            printf("VERTEX");
        else if (type == GL_FRAGMENT_SHADER)
            printf("FRAGMENT");
        printf(" sejder ima gresku! Greska: \n");
        printf(infoLog);
    }
    return shader;
}
unsigned int createShader(const char* vsSource, const char* fsSource)
{
    //Pravi objedinjeni sejder program koji se sastoji od Vertex sejdera ciji je kod na putanji vsSource

    unsigned int program; //Objedinjeni sejder
    unsigned int vertexShader; //Verteks sejder (za prostorne podatke)
    unsigned int fragmentShader; //Fragment sejder (za boje, teksture itd)

    program = glCreateProgram(); //Napravi prazan objedinjeni sejder program

    vertexShader = compileShader(GL_VERTEX_SHADER, vsSource); //Napravi i kompajliraj vertex sejder
    fragmentShader = compileShader(GL_FRAGMENT_SHADER, fsSource); //Napravi i kompajliraj fragment sejder

    //Zakaci verteks i fragment sejdere za objedinjeni program
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program); //Povezi ih u jedan objedinjeni sejder program
    glValidateProgram(program); //Izvrsi provjeru novopecenog programa

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_VALIDATE_STATUS, &success); //Slicno kao za sejdere
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(program, 512, NULL, infoLog);
        std::cout << "Objedinjeni sejder ima gresku! Greska: \n";
        std::cout << infoLog << std::endl;
    }

    //Posto su kodovi sejdera u objedinjenom sejderu, oni pojedinacni programi nam ne trebaju, pa ih brisemo zarad ustede na memoriji
    glDetachShader(program, vertexShader);
    glDeleteShader(vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(fragmentShader);

    return program;
}

unsigned char* rotateImage(const unsigned char* src, int w, int h, float angle)
{
    angle = angle * M_PI / 180.0f; // CCW rotacija (ulevo)
    float cosA = cos(angle);
    float sinA = sin(angle);

    int newW = w;
    int newH = h;

    unsigned char* dst = new unsigned char[newW * newH * 4];

    int cx = w / 2;
    int cy = h / 2;

    for (int y = 0; y < newH; y++) {
        for (int x = 0; x < newW; x++) {

            float nx = x - cx;
            float ny = y - cy;

            // inverse rotation
            float ox = nx * cosA - ny * sinA + cx;
            float oy = nx * sinA + ny * cosA + cy;

            int dstIndex = (y * newW + x) * 4;

            if (ox >= 0 && ox < w && oy >= 0 && oy < h) {
                int srcIndex = ((int)oy * w + (int)ox) * 4;
                dst[dstIndex + 0] = src[srcIndex + 0];
                dst[dstIndex + 1] = src[srcIndex + 1];
                dst[dstIndex + 2] = src[srcIndex + 2];
                dst[dstIndex + 3] = src[srcIndex + 3];
            }
            else {
                dst[dstIndex + 0] = 0;
                dst[dstIndex + 1] = 0;
                dst[dstIndex + 2] = 0;
                dst[dstIndex + 3] = 0;
            }
        }
    }

    return dst;
}


GLFWcursor* loadImageToCursor(const char* filePath) {
    int TextureWidth;
    int TextureHeight;
    int TextureChannels;

    unsigned char* ImageData = stbi_load(filePath, &TextureWidth, &TextureHeight, &TextureChannels, 0);

    unsigned char* Rotated = rotateImage(ImageData, TextureWidth, TextureHeight, 45.0f);

    if (ImageData != NULL)
    {
        GLFWimage image;
        image.width = TextureWidth;
        image.height = TextureHeight;
        image.pixels = Rotated;

        // Tacka na površini slike kursora koja se ponaša kao hitboks, moze se menjati po potrebi
        // Trenutno je gornji levi ugao, odnosno na 20% visine i 20% sirine slike kursora
        int hotspotX = TextureWidth / 5;
        int hotspotY = TextureHeight / 5;

        GLFWcursor* cursor = glfwCreateCursor(&image, hotspotX, hotspotY);

        delete[] Rotated;
        stbi_image_free(ImageData);
        std::cout << "Uspesno ucitan kursor!" << std::endl;
        return cursor;
    }
    else {
        std::cout << "Kursor nije ucitan! Putanja kursora: " << filePath << std::endl;
        stbi_image_free(ImageData);

    }
}
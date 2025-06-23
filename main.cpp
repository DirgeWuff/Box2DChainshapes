#include "raylib.h"
#include "box2d/types.h"
#include "box2d/box2d.h"
#include <random>
#include <vector>

constexpr int WINDOW_WIDTH = 640;
constexpr int WINDOW_HEIGHT = 480;
constexpr float PPM = 100.0f;
constexpr float TIME_STEP = 1.0f / 60.0f;
constexpr int SUB_STEP = 4;

// Convert meters to pixels using a b2Vec2
Vector2 m2PxVec(const b2Vec2 vec) {
    return Vector2{vec.x * PPM, vec.y * PPM};
}

// Convert pixels to meters using a Vector2
b2Vec2 px2MVec(const Vector2 vec) {
    return b2Vec2{vec.x / PPM, vec.y / PPM};
}

// Convert meters to pixels using a float
float m2Px(const float n) {
    return n * PPM;
}

// Convert pixels to meters using a float
float px2M(const float n) {
    return n / PPM;
}

// Generate random float between min and max
float getRandFloat(const float min, const float max) {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<float> distribution(min, max);
    return distribution(gen);
}

// Orb class
class Orb {
    float m_X{};
    float m_Y{};
    float m_radius{};

    b2Vec2 m_position{};
    b2BodyDef m_bodyDef{};
    b2BodyId m_body{};
    b2Circle m_circleShape{};
    b2ShapeDef m_shapeDef{};
    b2ShapeId m_shape{};

public:
    Orb() = default;

    Orb(
        const float centerX,
        const float centerY,
        const float radius,
        const b2WorldId world) :
            m_X(centerX),
            m_Y(centerY),
            m_radius(px2M(radius)),
            m_position{px2M(m_X), px2M(m_Y)},
            m_bodyDef(b2DefaultBodyDef())
    {
        m_bodyDef.position = m_position;
        m_bodyDef.type = b2_dynamicBody;
        m_body = b2CreateBody(world, &m_bodyDef);

        m_circleShape.center = {0.0f, 0.0f};
        m_circleShape.radius = m_radius;

        m_shapeDef = b2DefaultShapeDef();
        m_shape = b2CreateCircleShape(m_body, &m_shapeDef, &m_circleShape);
    }

    ~Orb() = default;

    static Orb* generateOrbAtMouse(const b2WorldId world) {
        const Vector2 mousePos = GetMousePosition();
        // Clion throwing a memory leak warning here, despite there being none.
        Orb* orb = new Orb(mousePos.x, mousePos.y, getRandFloat(5.0f, 25.0f), world);
        return orb;
    }

    void update() {
        m_position = b2Body_GetPosition(m_body);
    }

    void draw() const {
        DrawCircle(
            static_cast<int>(m2Px(m_position.x)),
            static_cast<int>(m2Px(m_position.y)),
            m2Px(m_radius),
            RED
            );
    }

    b2Vec2 getPosition() const {
        return m_position;
    }

    float getRadius() const {
        return m_radius;
    }
};

// Platform/chain shape class
class Platform {
    b2Vec2 m_verts[7]{};
    b2SurfaceMaterial m_chainMaterial{};
    b2SurfaceMaterial materials[5]{};
    b2BodyDef m_bodyDef{};
    b2BodyId m_bodyId{};
    b2ChainDef m_chainDef{};
    b2ChainId m_chainId{};
public:
    Platform() = default;

    explicit Platform(b2WorldId world) :
            m_bodyDef(b2DefaultBodyDef()),
            m_chainDef(b2DefaultChainDef())
    {
        m_bodyDef.type = b2_staticBody;
        m_bodyId = b2CreateBody(world, &m_bodyDef);
        // Bookended with verts offscreen, as chain shapes have no collision on isolated vertices
        m_verts[0] = px2MVec(Vector2{-10.0f, static_cast<float>(WINDOW_HEIGHT) - 10.0f});
        m_verts[1] = px2MVec(Vector2{0.0f, static_cast<float>(WINDOW_HEIGHT) - 30.0f});
        m_verts[2] = px2MVec(Vector2{160.0f, static_cast<float>(WINDOW_HEIGHT) - 90.0f});
        m_verts[3] = px2MVec(Vector2{320.0f, static_cast<float>(WINDOW_HEIGHT) - 75.0f});
        m_verts[4] = px2MVec(Vector2{480.0f, static_cast<float>(WINDOW_HEIGHT) - 140.0f});
        m_verts[5] = px2MVec(Vector2{static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT) - 30.0f});
        m_verts[6] = px2MVec(Vector2{static_cast<float>(WINDOW_WIDTH) + 10.0f, static_cast<float>(WINDOW_HEIGHT) - 10.0f});

        // Set up material
        m_chainMaterial = b2DefaultSurfaceMaterial();
        m_chainMaterial.friction = 0.2f;
        m_chainMaterial.restitution = 0.4f;

        // Create chain shape
        m_chainDef.count = sizeof(m_verts) / sizeof (m_verts[0]);
        m_chainDef.points = m_verts;
        m_chainDef.materials = &m_chainMaterial;
        m_chainDef.materialCount = 1;
        m_chainDef.isLoop = false;
        m_chainId = b2CreateChain(m_bodyId, &m_chainDef);
    }

    ~Platform() = default;

    void destroy() const {
        b2DestroyChain(m_chainId);
        b2DestroyBody(m_bodyId);
    }

    // Draw the chain
    void draw() const {
        for (std::size_t i = 0; i < m_chainDef.count - 1; i++) {
            DrawLineEx(
                m2PxVec(m_verts[i]),
                m2PxVec(m_verts[i + 1]),
                2.0f, {0, 0, 255, 255});
        }
    }
};


class World {
protected:
    b2WorldDef m_worldDef{};
    b2WorldId m_worldId{};
    Platform m_platform{};
    std::vector<Orb*> orbs = {};
    int orbClock;

public:
    World() :
        m_worldDef(b2DefaultWorldDef()),
        orbClock(0)
    {
        m_worldDef.gravity = {0.0f, 10.0f};
        m_worldId = b2CreateWorld(&m_worldDef);
        m_platform = Platform(m_worldId);
    };

    // Add orb if mouse button down, every other frame
    void handleInput() {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (orbClock <= 1) {
                orbClock++;
            }
            else {
                orbs.emplace_back(Orb::generateOrbAtMouse(m_worldId));
                orbClock = 0;
            }
        }
    }

    // Update orbs, delete/pop orb if off screen
    void update() {
        b2World_Step(m_worldId, TIME_STEP, SUB_STEP);

        for (std::size_t i = 0; i < orbs.size();) {
            orbs[i]->update();

            const float x = orbs[i]->getPosition().x;
            const float y = orbs[i]->getPosition().y;
            const float rad = orbs[i]->getRadius();

            if (x > WINDOW_WIDTH + rad || x < 0.0f - rad || y > WINDOW_WIDTH + rad) {
                delete orbs[i];
                orbs[i] = nullptr;
                orbs.erase(orbs.begin() + static_cast<long>(i));
            }
            else {
                i++;
            }
        }
    }

    void draw() const {
        for (const auto& orb : orbs) {
            orb->draw();
        }
        m_platform.draw();
    }

    void destroy() const {
        m_platform.destroy();
        b2DestroyWorld(m_worldId);
        for (const auto& orb : orbs) {
            delete orb;
        }
    }
};

int main() {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Box2D Polyline Demo");
    SetTargetFPS(60);

    auto world = World();

    while (!WindowShouldClose()) {
        world.handleInput();
        world.update();

        BeginDrawing();

        ClearBackground(BLACK);
        world.draw();
        EndDrawing();
    }

    world.destroy();

    return 0;
}
// #define PLATFORM_WEB
#include <raylib.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// Core simulation parameters
#define POP_SIZE 5 // Initial population size
#define INPUTS 17  // Neural network input count
#define HIDDEN 10  // Hidden layer neuron count
#define OUTPUTS 2  // Output neuron count (X and Y movement)

// Starting energy levels for different species
#define WOLF_STARTENERGY 150
#define FOX_STARTENERGY 120
#define DUCK_STARTENERGY 80
#define RABBIT_STARTENERGY 50

// Reproduction age for different species
#define WOLF_REPRODUCTIONAGE 15000
#define FOX_REPRODUCTIONAGE 10000
#define DUCK_REPRODUCTIONAGE 5500
#define RABBIT_REPRODUCTIONAGE 4500

// Speed of different species
#define WOLF_SPEED 10.0f
#define FOX_SPEED 10.2f
#define DUCK_SPEED 10.5f
#define RABBIT_SPEED 15.5f

// WINDOW SIZE
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1000

// Max hearth effects
#define MAX_HEARTH_EFFECTS 100

// Mutation chance for neural network weights
#define MUTATION_CHANCE 0.07f

// Species Types - defines the ecological role of each creature
typedef enum
{
    RABBIT, // Herbivore, eats grass
    DUCK,   // Herbivore, eats grass
    FOX,    // Predator, eats rabbits and ducks
    WOLF,   // Apex predator, eats rabbits, ducks, and foxes
    GRASS   // Producer, food for herbivores
} Species;

// Neural Network Structure - the "brain" of each creature
typedef struct
{
    float weightsIH[HIDDEN][INPUTS];  // Input -> Hidden layer weights
    float weightsHO[OUTPUTS][HIDDEN]; // Hidden -> Output layer weights
    float biasH[HIDDEN];              // Hidden layer bias values
    float biasO[OUTPUTS];             // Output layer bias values
} NeuralNetwork;

// Creature Structure - represents an individual in the simulation
typedef struct
{
    int age;             // Age of the creature
    Vector2 position;    // Position in 2D space
    float speed;         // Movement speed
    float energy;        // Current energy level
    Species type;        // Species type
    NeuralNetwork brain; // Neural network for decision making
    Color color;         // Visual representation color
    int last_mate;       // Last mate
} Creature;

typedef struct
{
    Vector2 position;
    int age;
} HearthEffect;

// Add this function somewhere in the code
float MutateValue(float value, float mutationRate)
{
    if ((float)rand() / RAND_MAX < mutationRate)
    {
        // Add or subtract up to 35% of the original value
        float mutation = ((float)rand() / RAND_MAX * 0.7f - 0.35f) * value;
        return value + mutation;
    }
    return value;
}

// Check if a creature has enough energy to reproduce
int CanReproduce(Creature *c)
{
    switch (c->type)
    {
    case RABBIT:
        return c->energy > RABBIT_STARTENERGY * .5f && c->age > RABBIT_REPRODUCTIONAGE && c->last_mate > RABBIT_REPRODUCTIONAGE;
    case DUCK:
        return c->energy > DUCK_STARTENERGY * .5f && c->age > DUCK_REPRODUCTIONAGE && c->last_mate > DUCK_REPRODUCTIONAGE;
    case FOX:
        return c->energy > FOX_STARTENERGY * .5f && c->age > FOX_REPRODUCTIONAGE && c->last_mate > FOX_REPRODUCTIONAGE;
    case WOLF:
        return c->energy > WOLF_STARTENERGY * .5f && c->age > WOLF_REPRODUCTIONAGE && c->last_mate > WOLF_REPRODUCTIONAGE;
    }
    return 0;
}

// Enhanced activation function using fast sigmoid approximation
float Activate(float x)
{
    // Clamp input to avoid overflow
    x = fmaxf(-10.0f, fminf(10.0f, x));

    // Fast sigmoid approximation: x / (1 + |x|)
    // Faster than standard sigmoid while maintaining similar shape
    float abs_x = fabsf(x);
    return 0.5f * (x / (1.0f + abs_x) + 1.0f);
}
void InitializeNetwork(NeuralNetwork *nn)
{
    // Initialize input->hidden weights and biases
    for (int i = 0; i < HIDDEN; i++)
    {
        for (int j = 0; j < INPUTS; j++)
        {
            // Generate random number between -1 and 1 with more variance
            float rand_val = 2.0f * ((float)rand() / RAND_MAX) - 1.0f;
            nn->weightsIH[i][j] = rand_val;
        }
        // Initialize biases with larger range
        nn->biasH[i] = (((float)rand() / RAND_MAX) * 2.0f - 1.0f);
    }

    // Initialize hidden->output weights and biases
    for (int i = 0; i < OUTPUTS; i++)
    {
        for (int j = 0; j < HIDDEN; j++)
        {
            float rand_val = 2.0f * ((float)rand() / RAND_MAX) - 1.0f;
            nn->weightsHO[i][j] = rand_val;
        }
        // Initialize output biases with larger range
        nn->biasO[i] = (((float)rand() / RAND_MAX) * 2.0f - 1.0f);
    }

    // Validate all values
    for (int i = 0; i < HIDDEN; i++)
    {
        for (int j = 0; j < INPUTS; j++)
        {
            if (isnan(nn->weightsIH[i][j]))
                nn->weightsIH[i][j] = 0.0f;
        }
        if (isnan(nn->biasH[i]))
            nn->biasH[i] = 0.0f;
    }

    for (int i = 0; i < OUTPUTS; i++)
    {
        for (int j = 0; j < HIDDEN; j++)
        {
            if (isnan(nn->weightsHO[i][j]))
                nn->weightsHO[i][j] = 0.0f;
        }
        if (isnan(nn->biasO[i]))
            nn->biasO[i] = 0.0f;
    }
}
// Define the linked list node structure for dynamic creature management
typedef struct CreatureNode
{
    Creature *data;            // Pointer to creature data
    struct CreatureNode *next; // Pointer to next node in list
} CreatureNode;

// Global head pointer for the linked list
CreatureNode *creatureList = NULL;

// Global array of hearth effects
HearthEffect hearthEffects[MAX_HEARTH_EFFECTS] = {0};

Texture2D RabbitIcon;
Texture2D DuckIcon;
Texture2D FoxIcon;
Texture2D WolfIcon;
Texture2D GrassIcon;

// Add a creature to the linked list
void AddCreature(Creature *creature)
{
    CreatureNode *newNode = (CreatureNode *)malloc(sizeof(CreatureNode));
    newNode->data = creature;
    newNode->next = NULL;

    // If list is empty, make the new node the head
    if (creatureList == NULL)
    {
        creatureList = newNode;
    }
    else
    {
        // Find the end of the list
        CreatureNode *current = creatureList;
        while (current->next != NULL)
        {
            current = current->next;
        }
        // Attach new node to the end
        current->next = newNode;
    }
}

// Initialize the starting population of creatures
void InitializeCreatures()
{
    // Clear existing list if any
    while (creatureList != NULL)
    {
        CreatureNode *temp = creatureList;
        creatureList = creatureList->next;
        free(temp);
    }

    // Create POP_SIZE creatures with varied properties
    for (int i = 0; i < POP_SIZE; i++)
    {
        Creature *newCreature;
        newCreature = (Creature *)malloc(sizeof(Creature));
        newCreature->age = 0;
        newCreature->last_mate = 0;
        // Random starting position
        newCreature->position = (Vector2){
            50 + rand() % (WINDOW_WIDTH - 100),
            50 + rand() % (WINDOW_HEIGHT - 100)};
        // Determine creature type by probability
        float r = (float)rand() / RAND_MAX;
        if (r < 0.5f)
            newCreature->type = RABBIT; // 40% chance
        // else if (r < 0.995f)
        //     newCreature->type = DUCK; // 30% chance
        // else if (r < 0.998f)
        //     newCreature->type = FOX; // 15% chance
        else
            newCreature->type = RABBIT; // 15% chance

        // Set starting energy based on species type
        switch (newCreature->type)
        {
        case RABBIT:
            newCreature->energy = RABBIT_STARTENERGY;
            newCreature->speed = RABBIT_SPEED + ((float)rand() / RAND_MAX) * 0.5f;
            break;
        case DUCK:
            newCreature->energy = DUCK_STARTENERGY;
            newCreature->speed = DUCK_SPEED + ((float)rand() / RAND_MAX) * 0.5f;
            break;
        case FOX:
            newCreature->energy = FOX_STARTENERGY;
            newCreature->speed = FOX_SPEED + ((float)rand() / RAND_MAX) * 0.5f;
            break;
        case WOLF:
            newCreature->energy = WOLF_STARTENERGY;
            newCreature->speed = WOLF_SPEED + ((float)rand() / RAND_MAX) * 0.5f;
            break;
        }

        // Initialize the neural network "brain"
        newCreature->brain = (NeuralNetwork){0};
        InitializeNetwork(&newCreature->brain);

        // Set color based on species type for visual identification
        newCreature->color = (newCreature->type == RABBIT) ? GREEN : (newCreature->type == DUCK) ? BLUE
                                                                 : (newCreature->type == FOX)    ? ORANGE
                                                                                                 : RED;
        AddCreature(newCreature);
    }
}

// Count the number of creatures in the linked list (debugging function)
void SizeOfCreatures()
{
    CreatureNode *current = creatureList;
    int size = 0;
    while (current != NULL)
    {
        size++;
        current = current->next;
    }
}

// Remove a creature from the simulation and free its memory
void DestroyCreature(Creature *creature)
{
    // Remove creature from linked list
    CreatureNode *current = creatureList;
    CreatureNode *prev = NULL;

    SizeOfCreatures();
    while (current != NULL)
    {
        if (current->data == creature)
        {
            // Handle removal at the head of the list
            if (prev == NULL)
            {
                creatureList = current->next;
            }
            else
            {
                prev->next = current->next;
            }
            CreatureNode *temp = current;
            current = current->next;
            free(temp->data); // Free creature data
            free(temp);       // Free node itself
            return;
        }
        prev = current;
        current = current->next;
    }
    SizeOfCreatures();
}

// Check for interactions between creatures (eating, reproduction)
void CheckInteractions(Creature *current)
{
    CreatureNode *other = creatureList;
    while (other != NULL)
    {
        if (current != other->data && other->data->energy > 0)
        {
            // Calculate distance between creatures
            float distance = sqrtf(powf(current->position.x - other->data->position.x, 2) +
                                   powf(current->position.y - other->data->position.y, 2));

            // Interaction occurs when creatures are close enough
            if (distance < 24)
            {
                // Fox eats rabbits and ducks
                if (current->type == FOX && (other->data->type == DUCK || other->data->type == RABBIT))
                {
                    current->energy += other->data->energy;
                    other->data->energy = -1; // Mark for removal
                }
                // Wolf eats rabbits, ducks, and foxes
                else if (current->type == WOLF && (other->data->type == DUCK || other->data->type == RABBIT || other->data->type == FOX))
                {
                    current->energy += other->data->energy;
                    other->data->energy = -1; // Mark for removal
                }
                // Duck eats grass
                else if (current->type == DUCK && other->data->type == GRASS)
                {
                    current->energy += other->data->energy;
                    other->data->energy = -1; // Mark for removal
                }
                // Rabbit eats grass
                else if (current->type == RABBIT && other->data->type == GRASS)
                {
                    current->energy += other->data->energy;
                    other->data->energy = -1; // Mark for removal
                }
                // Reproduction between same species if they have enough energy
                else if (current->type == other->data->type &&
                         CanReproduce(current) &&
                         CanReproduce(other->data) &&
                         current->type != GRASS)
                {
                    // 70% chance to reproduce when conditions are met
                    if ((float)rand() / RAND_MAX < 0.7f)
                    {
                        // Create offspring with traits from both parents
                        Creature *offspring = (Creature *)malloc(sizeof(Creature));
                        offspring->type = current->type;
                        offspring->speed = current->speed;
                        offspring->color = current->color;

                        // Mix neural networks from both parents
                        for (int i = 0; i < HIDDEN; i++)
                        {
                            for (int j = 0; j < INPUTS; j++)
                            {
                                // 50% chance to inherit from each parent
                                offspring->brain.weightsIH[i][j] = MutateValue(
                                    (rand() % 2) ? current->brain.weightsIH[i][j] : other->data->brain.weightsIH[i][j],
                                    MUTATION_CHANCE);
                            }
                            offspring->brain.biasH[i] = MutateValue(
                                (rand() % 2) ? current->brain.biasH[i] : other->data->brain.biasH[i],
                                MUTATION_CHANCE);
                        }

                        for (int i = 0; i < OUTPUTS; i++)
                        {
                            for (int j = 0; j < HIDDEN; j++)
                            {
                                // CORRECT: Using weightsHO to copy hidden-to-output weights
                                offspring->brain.weightsHO[i][j] = MutateValue(
                                    (rand() % 2) ? current->brain.weightsHO[i][j] : other->data->brain.weightsHO[i][j],
                                    MUTATION_CHANCE);
                            }
                            // CORRECT: Using biasO for output layer biases
                            offspring->brain.biasO[i] = MutateValue(
                                (rand() % 2) ? current->brain.biasO[i] : other->data->brain.biasO[i],
                                MUTATION_CHANCE);
                        }

                        // Position offspring near parents with slight randomness
                        offspring->position = (Vector2){
                            current->position.x + ((float)rand() / RAND_MAX * 40 - 20),
                            current->position.y + ((float)rand() / RAND_MAX * 40 - 20)};

                        // Transfer energy from parents to offspring
                        float parentEnergy1 = current->energy / 3;
                        float parentEnergy2 = other->data->energy / 3;

                        if (isnan(parentEnergy1) || isnan(parentEnergy2))
                        {
                            parentEnergy1 = fmaxf(0, current->energy / 3);
                            parentEnergy2 = fmaxf(0, other->data->energy / 3);
                        }

                        offspring->energy = parentEnergy1 + parentEnergy2;
                        current->energy = current->energy * 2 / 3;
                        other->data->energy = other->data->energy * 2 / 3;
                        offspring->age = 0;
                        offspring->last_mate = 0;
                        // Add offspring to the simulation
                        AddCreature(offspring);
                        // Add hearth effect
                        for (size_t i = 0; i < MAX_HEARTH_EFFECTS; i++)
                        {
                            if (hearthEffects[i].age == 0 && hearthEffects[i].position.x == 0 && hearthEffects[i].position.y == 0)
                            {
                                hearthEffects[i].age = 1;
                                hearthEffects[i].position = offspring->position;
                                break;
                            }
                        }
                    }
                }
            }
        }
        other = other->next;
    }
}

// Process neural network inputs to determine creature movement and actions
void ProcessNeuralNetwork(Creature *c, float inputs[INPUTS])
{
    // Initialize and validate inputs
    for (int i = 0; i < INPUTS; i++)
    {
        inputs[i] = 0.0f;
    }

    // Basic environmental inputs
    inputs[0] = c->position.x / WINDOW_WIDTH;  // Normalized x position
    inputs[1] = c->position.y / WINDOW_HEIGHT; // Normalized y position
    inputs[2] = c->energy / 1000.0f;           // Normalized energy level
    // Replace age with boundary proximity (how close to edge of simulation)
    float distToLeftBoundary = c->position.x;
    float distToRightBoundary = WINDOW_WIDTH - c->position.x;
    float distToTopBoundary = c->position.y;
    float distToBottomBoundary = WINDOW_HEIGHT - c->position.y;
    float closestBoundaryDist = fminf(fminf(distToLeftBoundary, distToRightBoundary),
                                      fminf(distToTopBoundary, distToBottomBoundary));
    inputs[3] = fminf(1.0f, closestBoundaryDist / 100.0f); // Normalized boundary proximity

    // Initialize tracking variables
    float nearestFoodDist = INFINITY;
    float nearestPredatorDist = INFINITY;
    float nearestMateDist = INFINITY;
    Vector2 foodDir = {0, 0};
    Vector2 predatorDir = {0, 0};
    Vector2 mateDir = {0, 0};

    // Scan surroundings with improved range detection
    const float MAX_DETECTION_RANGE = 1000.0f; // Maximum detection range
    CreatureNode *other = creatureList;
    while (other != NULL)
    {
        if (other->data != c && other->data->energy > 0)
        {
            float dist = sqrtf(powf(c->position.x - other->data->position.x, 2) +
                               powf(c->position.y - other->data->position.y, 2));

            if (dist <= MAX_DETECTION_RANGE)
            {
                Vector2 direction = {
                    other->data->position.x - c->position.x,
                    other->data->position.y - c->position.y};

                // Food detection with species-specific logic
                if (((c->type == RABBIT || c->type == DUCK) && other->data->type == GRASS) ||
                    (c->type == FOX && (other->data->type == RABBIT || other->data->type == DUCK)) ||
                    (c->type == WOLF && (other->data->type == RABBIT || other->data->type == DUCK || other->data->type == FOX)))
                {
                    if (dist < nearestFoodDist)
                    {
                        nearestFoodDist = dist;
                        foodDir = direction;
                    }
                }

                // Predator detection
                if (((c->type == RABBIT || c->type == DUCK) && (other->data->type == FOX || other->data->type == WOLF)) ||
                    (c->type == FOX && other->data->type == WOLF))
                {
                    if (dist < nearestPredatorDist)
                    {
                        nearestPredatorDist = dist;
                        predatorDir = direction;
                    }
                }

                // Mate detection (same species and suitable for reproduction)
                if (other->data->type == c->type && CanReproduce(other->data))
                {
                    if (dist < nearestMateDist)
                    {
                        nearestMateDist = dist;
                        mateDir = direction;
                    }
                }
            }
        }
        other = other->next;
    }

    // Normalize and set sensory inputs
    // Food inputs (4-7)
    // Amplify the food proximity signal with a multiplier
    float foodProximityMultiplier = 25.0f; // Increase to make food more attractive
    inputs[4] = (nearestFoodDist == INFINITY) ? 0.0f : fminf(1.0f, (1.0f - (nearestFoodDist / MAX_DETECTION_RANGE)) * foodProximityMultiplier);

    // Amplify directional signals to make creatures navigate more purposefully toward food
    float foodDirectionMultiplier = 25.0f; // Strengthen directional pull toward food
    inputs[5] = (nearestFoodDist == INFINITY) ? 0.0f : (foodDir.x / nearestFoodDist) * foodDirectionMultiplier;
    inputs[6] = (nearestFoodDist == INFINITY) ? 0.0f : (foodDir.y / nearestFoodDist) * foodDirectionMultiplier;

    // Adjust hunger threshold - make creatures seek food even at higher energy levels
    inputs[7] = c->energy < 150.0f ? (1.0f - (c->energy / 150.0f)) : 0.0f; // Progressive hunger signal

    // Predator inputs (8-11)
    inputs[8] = (nearestPredatorDist == INFINITY) ? 0.0f : 25.0f - (nearestPredatorDist / MAX_DETECTION_RANGE);
    inputs[9] = (nearestPredatorDist == INFINITY) ? 0.0f : predatorDir.x / nearestPredatorDist;
    inputs[10] = (nearestPredatorDist == INFINITY) ? 0.0f : predatorDir.y / nearestPredatorDist;
    inputs[11] = c->energy < 30.0f ? 5.0f : 0.0f; // Weakness signal

    // Mate inputs (12-15)
    inputs[12] = (nearestMateDist == INFINITY) ? 0.0f : 25.0f - (nearestMateDist / MAX_DETECTION_RANGE);
    inputs[13] = (nearestMateDist == INFINITY) ? 0.0f : mateDir.x / nearestMateDist;
    inputs[14] = (nearestMateDist == INFINITY) ? 0.0f : mateDir.y / nearestMateDist;
    inputs[15] = CanReproduce(c) ? 10.0f : 0.0f; // Reproduction readiness

    // Environmental awareness (16)
    inputs[16] = c->speed / 15.5f; // Normalized speed

    // Process inputs through neural network's hidden layer
    float hidden[HIDDEN];
    for (int i = 0; i < HIDDEN; i++)
    {
        float sum = c->brain.biasH[i];
        for (int j = 0; j < INPUTS; j++)
        {
            sum += c->brain.weightsIH[i][j] * inputs[j];
        }
        hidden[i] = Activate(sum);
    }

    // Process hidden layer outputs to produce final outputs
    float output[OUTPUTS];
    for (int i = 0; i < OUTPUTS; i++)
    {
        float sum = c->brain.biasO[i];
        for (int j = 0; j < HIDDEN; j++)
        {
            sum += c->brain.weightsHO[i][j] * hidden[j];
        }
        output[i] = Activate(sum);
    }

    // Update position based on neural network output
    // Output values are between 0-1, so subtract 0.5 to allow negative movement
    // Update position based on neural network output

    // Check for NaN values and replace them with defaults
    if (isnan(output[0]) || isnan(output[1]))
    {
        output[0] = 0.5f;
        output[1] = 0.5f;
    }

    c->position.x += (output[0] - 0.5f) * c->speed;
    c->position.y += (output[1] - 0.5f) * c->speed;

    // Clamp positions to window boundaries
    c->position.x = fminf(fmaxf(c->position.x, 32), WINDOW_WIDTH - 32);
    c->position.y = fminf(fmaxf(c->position.y, 32), WINDOW_HEIGHT - 32);

    // Calculate movement cost with validation
    float movementX = (output[0] - 0.5f) * c->speed;
    float movementY = (output[1] - 0.5f) * c->speed;

    // Validate to avoid extreme values
    if (isnan(movementX) || isnan(movementY))
    {
        movementX = 0;
        movementY = 0;
    }

    float movementCost = fabsf(movementX) + fabsf(movementY);

    // Different species have different energy efficiencies
    float energyCost;
    switch (c->type)
    {
    case RABBIT:
        energyCost = 0.02f; // Rabbits use less energy
        break;
    case DUCK:
        energyCost = 0.03f; // Ducks use slightly more energy
        break;
    case FOX:
        energyCost = 0.06f; // Foxes use more energy
        break;
    case WOLF:
        energyCost = 0.09f; // Wolves use the most energy
        break;
    }
    c->energy -= movementCost * energyCost;
    // Validate energy to prevent NaN
    if (isnan(c->energy))
    {
        c->energy = -1; // Mark for removal
    }

    // Ensure energy is within reasonable bounds
    c->energy = fminf(c->energy, 1000.0f); // Cap maximum energy
}

// Update all creatures in the simulation for one frame
void UpdateCreatures()
{
    CreatureNode *current = creatureList;
    while (current != NULL)
    {
        if (current->data->type != GRASS)
        {
            // Create an empty array for inputs
            float inputs[INPUTS] = {0};

            // Process neural network (inputs are populated inside the function)
            ProcessNeuralNetwork(current->data, inputs);
        }
        current->data->age++;
        current->data->last_mate++;

        if (current->data->type != GRASS)
        {
            current->data->energy -= 0.005f; // Energy cost for existing
        }
        // Safety validation for creature data
        if (isnan(current->data->energy) || isnan(current->data->position.x) || isnan(current->data->position.y))
        {
            DestroyCreature(current->data);
            return;
        }
        // Check for interactions with other creatures
        CheckInteractions(current->data);

        // Remove creatures with no energy
        if (current->data->energy <= 0)
        {
            DestroyCreature(current->data);
            return;
        }
        current = current->next;
    }

    // Add random grass
    if ((float)rand() / RAND_MAX < 0.06f)
    {
        Creature *grass = (Creature *)malloc(sizeof(Creature));
        grass->position = (Vector2){50 + rand() % (WINDOW_WIDTH - 100), 50 + rand() % (WINDOW_HEIGHT - 100)};
        grass->speed = 0;
        grass->energy = 30;
        grass->type = GRASS;
        grass->age = 0;
        grass->last_mate = 0;
        grass->color = DARKGREEN;
        AddCreature(grass);
    }
}

// Draw all creatures to the screen
void DrawCreatures()
{
    CreatureNode *current = creatureList;
    while (current != NULL)
    {
        // Draw energy level as text
        DrawText(TextFormat("e:%.0f a:%d", current->data->energy, current->data->age),
                 (int)(current->data->position.x - 17),
                 (int)(current->data->position.y + 17),
                 10,
                 current->data->color);

        // Draw creature based on type
        switch (current->data->type)
        {
        case RABBIT:
            DrawTexturePro(RabbitIcon,
                           (Rectangle){0, 0, RabbitIcon.width, RabbitIcon.height},
                           (Rectangle){current->data->position.x - 16, current->data->position.y - 16, 32, 32},
                           (Vector2){0, 0}, 0, WHITE);
            break;
        case DUCK:
            DrawTexturePro(DuckIcon,
                           (Rectangle){0, 0, DuckIcon.width, DuckIcon.height},
                           (Rectangle){current->data->position.x - 16, current->data->position.y - 16, 32, 32},
                           (Vector2){0, 0}, 0, WHITE);
            break;
        case FOX:
            DrawTexturePro(FoxIcon,
                           (Rectangle){0, 0, FoxIcon.width, FoxIcon.height},
                           (Rectangle){current->data->position.x - 16, current->data->position.y - 16, 32, 32},
                           (Vector2){0, 0}, 0, WHITE);
            break;
        case WOLF:
            DrawTexturePro(WolfIcon,
                           (Rectangle){0, 0, WolfIcon.width, WolfIcon.height},
                           (Rectangle){current->data->position.x - 16, current->data->position.y - 16, 32, 32},
                           (Vector2){0, 0}, 0, WHITE);
            break;
        case GRASS:
            DrawTexturePro(GrassIcon,
                           (Rectangle){0, 0, GrassIcon.width, GrassIcon.height},
                           (Rectangle){current->data->position.x - 16, current->data->position.y - 16, 32, 32},
                           (Vector2){0, 0}, 0, WHITE);
            break;
        }
        current = current->next;
    }
    for (size_t i = 0; i < MAX_HEARTH_EFFECTS; i++)
    {
        if (hearthEffects[i].age > 0 && hearthEffects[i].age < 500)
        {
            DrawText("sex", hearthEffects[i].position.x, hearthEffects[i].position.y, 20, RED);
            hearthEffects[i].age++;
        }
        else if (hearthEffects[i].age >= 500)
        {
            hearthEffects[i].age = 0;
            hearthEffects[i].position = (Vector2){0, 0};
        }
    }
}

// Main program entry point
int main()
{
    // Initialize the window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    // SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Evolution Simulator");
    srand(time(NULL)); // Seed the random number generator

    // Load textures
    RabbitIcon = LoadTexture("./assets/rabbit.png");
    DuckIcon = LoadTexture("./assets/duck.png");
    FoxIcon = LoadTexture("./assets/fox.png");
    WolfIcon = LoadTexture("./assets/wolf.png");
    GrassIcon = LoadTexture("./assets/grass.png");

    // Setup the initial population
    InitializeCreatures();
    printf("Creatures initialized\n");
    printf("Creatures: %d\n", POP_SIZE);
    SetTargetFPS(240); // Higher FPS for faster simulation

    static Species selectedSpecies = -1;
    static int dragEnabled = 0;
    static int cloneEnabled = 0;
    static Creature *draggedCreature = NULL;

    // Main game loop
    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Update and render all creatures
        UpdateCreatures();
        DrawCreatures();
        // Count creatures by type
        int counts[5] = {0}; // RABBIT, DUCK, FOX, WOLF, GRASS
        CreatureNode *countCurrent = creatureList;
        while (countCurrent != NULL)
        {
            if (countCurrent->data->type < 5)
            { // Safety check
                counts[countCurrent->data->type]++;
            }
            countCurrent = countCurrent->next;
        }

        // Display population statistics
        DrawText(TextFormat("Rabbits: %d", counts[RABBIT]), 10, 10, 20, GREEN);
        DrawText(TextFormat("Ducks: %d", counts[DUCK]), 10, 35, 20, BLUE);
        DrawText(TextFormat("Foxes: %d", counts[FOX]), 10, 60, 20, ORANGE);
        DrawText(TextFormat("Wolves: %d", counts[WOLF]), 10, 85, 20, RED);
        DrawText(TextFormat("Grass: %d", counts[GRASS]), 10, 110, 20, DARKGREEN);

        // Define button areas
        Rectangle rabbitBtn = {200, 10, 100, 20};
        Rectangle duckBtn = {200, 35, 100, 20};
        Rectangle foxBtn = {200, 60, 100, 20};
        Rectangle wolfBtn = {200, 85, 100, 20};
        Rectangle clearBtn = {200, 110, 100, 20};
        Rectangle toggleDragBtn = {200, 135, 100, 20};
        Rectangle toggleCloneBtn = {200, 160, 100, 20};

        // Draw buttons
        DrawRectangleRec(rabbitBtn, LIGHTGRAY);
        DrawRectangleRec(duckBtn, LIGHTGRAY);
        DrawRectangleRec(foxBtn, LIGHTGRAY);
        DrawRectangleRec(wolfBtn, LIGHTGRAY);
        DrawRectangleRec(clearBtn, LIGHTGRAY);
        DrawRectangleRec(toggleDragBtn, dragEnabled ? GRAY : LIGHTGRAY);
        DrawRectangleRec(toggleCloneBtn, cloneEnabled ? GRAY : LIGHTGRAY);

        DrawText("Add Rabbit", 205, 12, 16, GREEN);
        DrawText("Add Duck", 205, 37, 16, BLUE);
        DrawText("Add Fox", 205, 62, 16, ORANGE);
        DrawText("Add Wolf", 205, 87, 16, RED);
        DrawText("Clear Selection", 205, 112, 16, MAROON);
        DrawText("Toggle Drag", 205, 137, 16, MAROON);
        DrawText("Toggle Cloning", 205, 162, 16, MAROON);

        Vector2 mousePos = GetMousePosition();

        // Handle drag and drop functionality
        if (dragEnabled)
        {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                // Check if clicked on a creature
                CreatureNode *current = creatureList;
                while (current != NULL)
                {
                    float dist = sqrtf(powf(mousePos.x - current->data->position.x, 2) +
                                       powf(mousePos.y - current->data->position.y, 2));
                    if (dist < 32)
                    { // Assuming creature radius is 32
                        draggedCreature = current->data;
                        break;
                    }
                    current = current->next;
                }
            }
            else if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && draggedCreature != NULL)
            {
                // Update dragged creature position
                draggedCreature->position = mousePos;
            }
            else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
            {
                draggedCreature = NULL;
            }
        }

        // Cloning
        if (cloneEnabled)
        {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                // Check if clicked on a creature
                CreatureNode *current = creatureList;
                while (current != NULL)
                {
                    float dist = sqrtf(powf(mousePos.x - current->data->position.x, 2) +
                                       powf(mousePos.y - current->data->position.y, 2));
                    if (dist < 32)
                    { // Assuming creature radius is 32
                        Creature *newCreature = (Creature *)malloc(sizeof(Creature));
                        newCreature->position = (Vector2){
                            50 + rand() % (WINDOW_WIDTH - 100),
                            50 + rand() % (WINDOW_HEIGHT - 100)};
                        newCreature->type = current->data->type;
                        newCreature->age = 0;
                        newCreature->last_mate = 0;
                        newCreature->energy = current->data->energy;
                        newCreature->speed = current->data->speed;
                        newCreature->color = current->data->color;

                        // Deep copy of the neural network
                        for (int i = 0; i < HIDDEN; i++)
                        {
                            for (int j = 0; j < INPUTS; j++)
                            {
                                newCreature->brain.weightsIH[i][j] = current->data->brain.weightsIH[i][j];
                            }
                            newCreature->brain.biasH[i] = current->data->brain.biasH[i];
                        }
                        for (int i = 0; i < OUTPUTS; i++)
                        {
                            for (int j = 0; j < HIDDEN; j++)
                            {
                                newCreature->brain.weightsHO[i][j] = current->data->brain.weightsHO[i][j];
                            }
                            newCreature->brain.biasO[i] = current->data->brain.biasO[i];
                        }

                        AddCreature(newCreature);
                        break;
                    }
                    current = current->next;
                }
            }
        }

        // Check button clicks
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (CheckCollisionPointRec(mousePos, rabbitBtn))
                selectedSpecies = RABBIT;
            else if (CheckCollisionPointRec(mousePos, duckBtn))
                selectedSpecies = DUCK;
            else if (CheckCollisionPointRec(mousePos, foxBtn))
                selectedSpecies = FOX;
            else if (CheckCollisionPointRec(mousePos, wolfBtn))
                selectedSpecies = WOLF;
            else if (CheckCollisionPointRec(mousePos, clearBtn))
                selectedSpecies = -1;
            else if (CheckCollisionPointRec(mousePos, toggleDragBtn))
                dragEnabled = !dragEnabled;
            else if (CheckCollisionPointRec(mousePos, toggleCloneBtn))
                cloneEnabled = !cloneEnabled;
            else if (selectedSpecies != -1 && !dragEnabled)
            {
                // Create new creature at click location
                Creature *newCreature = (Creature *)malloc(sizeof(Creature));
                newCreature->position = mousePos;
                newCreature->type = selectedSpecies;
                newCreature->age = 0;
                newCreature->last_mate = 0;

                switch (selectedSpecies)
                {
                case RABBIT:
                    newCreature->energy = RABBIT_STARTENERGY;
                    newCreature->speed = 2.5f;
                    newCreature->color = GREEN;
                    break;
                case DUCK:
                    newCreature->energy = DUCK_STARTENERGY;
                    newCreature->speed = 1.5f;
                    newCreature->color = BLUE;
                    break;
                case FOX:
                    newCreature->energy = FOX_STARTENERGY;
                    newCreature->speed = 1.2f;
                    newCreature->color = ORANGE;
                    break;
                case WOLF:
                    newCreature->energy = WOLF_STARTENERGY;
                    newCreature->speed = 1.0f;
                    newCreature->color = RED;
                    break;
                }

                InitializeNetwork(&newCreature->brain);
                AddCreature(newCreature);
            }
        }

        // Show drag mode status
        if (dragEnabled)
        {
            DrawText("Drag Mode: ON", WINDOW_WIDTH - 200, 10, 20, RED);
        }

        DrawText(TextFormat("FPS: %d", GetFPS()), WINDOW_WIDTH - 100, 40, 20, LIME);

        EndDrawing();
    }

    // Cleanup
    CloseWindow();
    return 0;
}
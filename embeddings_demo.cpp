/**
 * Embeddings Demo - FastEmbed Integration for Semantic Search
 *
 * Demonstrates cost-effective, high-performance embeddings using FastEmbed
 * with semantic search capabilities for regulatory document analysis.
 *
 * Features:
 * - Document chunking and processing
 * - Embedding generation with multiple models
 * - Semantic similarity search
 * - Performance benchmarking
 * - Regulatory document analysis
 */

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>

#include "shared/llm/embeddings_client.hpp"
#include "shared/config/configuration_manager.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/error_handler.hpp"

using namespace regulens;

/**
 * @brief Sample regulatory documents for testing
 */
const std::vector<std::pair<std::string, std::string>> SAMPLE_DOCUMENTS = {
    {"doc_001", R"(
Anti-Money Laundering Compliance Program

This document outlines the comprehensive AML compliance program designed to prevent,
detect, and report money laundering activities in accordance with regulatory requirements.

Key Components:
1. Customer Due Diligence (CDD) procedures
2. Enhanced Due Diligence (EDD) for high-risk customers
3. Transaction monitoring systems
4. Suspicious Activity Reporting (SAR) processes
5. Risk Assessment methodologies

The program ensures compliance with BSA, OFAC, and FinCEN regulations while maintaining
efficient operations and customer service standards.
    )"},

    {"doc_002", R"(
Know Your Customer (KYC) Requirements

Financial institutions must implement robust KYC procedures to verify customer identities
and assess risk profiles before establishing business relationships.

Required Documentation:
- Government-issued photo ID
- Proof of address
- Source of funds verification
- Beneficial ownership information
- Risk assessment questionnaires

Failure to comply with KYC requirements may result in significant regulatory penalties
and reputational damage to the institution.
    )"},

    {"doc_003", R"(
Regulatory Reporting Obligations

Financial institutions are required to file various reports with regulatory authorities
to ensure transparency and compliance monitoring.

Key Reports:
- Currency Transaction Reports (CTR)
- Suspicious Activity Reports (SAR)
- Foreign Bank Account Reports (FBAR)
- Cash Transaction Reports
- Monetary Instrument Logs

Timely and accurate reporting is essential for maintaining regulatory compliance and
avoiding enforcement actions.
    )"},

    {"doc_004", R"(
Risk-Based Compliance Framework

A risk-based approach to compliance focuses resources on the highest-risk areas
while maintaining appropriate controls for lower-risk activities.

Risk Factors to Consider:
- Customer risk profiles
- Geographic risk locations
- Product and service complexity
- Transaction volumes and amounts
- Third-party relationships

Regular risk assessments and control testing ensure the effectiveness of the
compliance program.
    )"},

    {"doc_005", R"(
Transaction Monitoring Systems

Automated systems designed to detect unusual or suspicious transaction patterns
that may indicate money laundering or other financial crimes.

Monitoring Capabilities:
- Velocity and frequency analysis
- Geographic analysis
- Amount threshold monitoring
- Peer group comparisons
- Behavioral pattern recognition

Effective transaction monitoring requires regular rule tuning and false positive
reduction strategies.
    )"}
};

/**
 * @brief Benchmark embedding generation performance
 */
void benchmark_embeddings(std::shared_ptr<EmbeddingsClient> client,
                         const std::vector<std::string>& texts) {

    std::cout << "\n🔬 Benchmarking Embedding Generation\n";
    std::cout << "=====================================\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    EmbeddingRequest request{texts};
    auto response = client->generate_embeddings(request);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    if (response) {
        std::cout << "✅ Generated embeddings for " << texts.size() << " texts\n";
        std::cout << "📏 Embedding dimensions: " << (response->embeddings.empty() ? 0 : response->embeddings[0].size()) << "\n";
        std::cout << "⏱️  Total processing time: " << response->processing_time_ms << "ms\n";
        std::cout << "📊 Average time per text: " << (response->processing_time_ms / texts.size()) << "ms\n";
        std::cout << "🔢 Estimated tokens processed: " << response->total_tokens << "\n";
    } else {
        std::cout << "❌ Failed to generate embeddings\n";
    }
}

/**
 * @brief Demonstrate semantic search capabilities
 */
void demonstrate_semantic_search(std::shared_ptr<EmbeddingsClient> embeddings_client,
                               std::shared_ptr<DocumentProcessor> doc_processor,
                               std::shared_ptr<SemanticSearchEngine> search_engine) {

    std::cout << "\n🔍 Semantic Search Demonstration\n";
    std::cout << "=================================\n";

    // Add sample documents to search index
    std::cout << "📚 Indexing sample regulatory documents...\n";
    for (const auto& [doc_id, doc_text] : SAMPLE_DOCUMENTS) {
        if (search_engine->add_document(doc_text, doc_id)) {
            std::cout << "  ✅ Indexed: " << doc_id << "\n";
        } else {
            std::cout << "  ❌ Failed to index: " << doc_id << "\n";
        }
    }

    // Test semantic search queries
    std::vector<std::string> test_queries = {
        "How do I implement AML compliance?",
        "What documents are needed for KYC?",
        "When should I file suspicious activity reports?",
        "How to assess customer risk levels?",
        "What are the requirements for transaction monitoring?"
    };

    std::cout << "\n🔎 Performing semantic searches...\n\n";

    for (const auto& query : test_queries) {
        std::cout << "Query: \"" << query << "\"\n";
        std::cout << "Results:\n";

        auto results = search_engine->semantic_search(query, 2, 0.3f);

        if (results.empty()) {
            std::cout << "  No relevant documents found\n";
        } else {
            for (size_t i = 0; i < results.size(); ++i) {
                const auto& result = results[i];
                std::cout << "  " << (i + 1) << ". " << result.document_id
                         << " (similarity: " << std::fixed << std::setprecision(3)
                         << result.similarity_score << ")\n";
                std::cout << "     \"" << result.chunk_text.substr(0, 100) << "...\"\n";
            }
        }
        std::cout << "\n";
    }
}

/**
 * @brief Demonstrate document processing capabilities
 */
void demonstrate_document_processing(std::shared_ptr<DocumentProcessor> processor) {

    std::cout << "\n📄 Document Processing Demonstration\n";
    std::cout << "=====================================\n";

    std::string sample_text = R"(
This is the first sentence of our regulatory document. It discusses important compliance requirements that all financial institutions must follow.

This is a second paragraph that explains additional details about the compliance framework. It covers multiple aspects including risk assessment, monitoring procedures, and reporting obligations.

The third paragraph provides specific examples of regulatory requirements. These include customer due diligence, transaction monitoring, and suspicious activity reporting. Each requirement has specific timelines and documentation standards that must be met.

Finally, this last paragraph summarizes the key takeaways and provides guidance for implementation. Organizations should maintain comprehensive documentation and regularly review their compliance programs to ensure ongoing effectiveness.
    )";

    std::cout << "Original text length: " << sample_text.length() << " characters\n";

    // Test different chunking strategies
    std::vector<std::string> strategies = {"sentence", "paragraph", "fixed"};

    for (const auto& strategy : strategies) {
        DocumentChunkingConfig config;
        config.chunking_strategy = strategy;
        config.chunk_size = 100; // Smaller for demo

        auto chunks = processor->process_document(sample_text, "demo_doc", config);

        std::cout << "\n📋 Chunking strategy: " << strategy << "\n";
        std::cout << "Generated " << chunks.size() << " chunks:\n";

        for (size_t i = 0; i < std::min(chunks.size(), size_t(3)); ++i) {
            std::cout << "  Chunk " << (i + 1) << ": \"" << chunks[i].text.substr(0, 80) << "...\"\n";
            std::cout << "    Tokens: ~" << processor->estimate_token_count(chunks[i].text) << "\n";
        }
    }
}

/**
 * @brief Demonstrate embedding similarity calculations
 */
void demonstrate_similarity_calculations(std::shared_ptr<EmbeddingsClient> client) {

    std::cout << "\n📏 Embedding Similarity Demonstration\n";
    std::cout << "=====================================\n";

    std::vector<std::string> texts = {
        "Anti-money laundering compliance procedures",
        "AML compliance and regulatory requirements",
        "Customer identification and verification processes",
        "Financial transaction monitoring systems",
        "Regulatory reporting obligations",
        "Cooking recipes and food preparation"
    };

    auto response = client->generate_embeddings({texts});
    if (!response || response->embeddings.size() != texts.size()) {
        std::cout << "❌ Failed to generate embeddings for similarity test\n";
        return;
    }

    std::cout << "Computing similarity matrix:\n\n";

    for (size_t i = 0; i < texts.size(); ++i) {
        std::cout << "Text " << (i + 1) << ": " << texts[i].substr(0, 50) << "...\n";

        // Find most similar texts
        auto similarities = client->find_most_similar(response->embeddings[i],
                                                    response->embeddings, 3);

        for (const auto& [idx, similarity] : similarities) {
            if (idx != i) { // Skip self-similarity
                std::cout << "  Similar to Text " << (idx + 1) << ": "
                         << std::fixed << std::setprecision(3) << similarity << "\n";
            }
        }
        std::cout << "\n";
    }
}

/**
 * @brief Main demonstration function
 */
void demonstrate_embeddings() {
    std::cout << "🧠 Advanced Embeddings Integration Demo\n";
    std::cout << "=======================================\n";
    std::cout << "Using FastEmbed for cost-effective, high-performance embeddings\n\n";

    // Initialize components
    auto config = std::make_shared<ConfigurationManager>();
    auto logger = std::make_shared<StructuredLogger>();
    auto error_handler = std::make_shared<ErrorHandler>(config, logger.get());

    // Initialize embeddings client
    auto embeddings_client = create_embeddings_client(config, logger.get(), error_handler.get());
    if (!embeddings_client) {
        std::cout << "❌ Failed to initialize embeddings client\n";
        return;
    }

    // Initialize document processor
    auto doc_processor = create_document_processor(config, logger.get(), error_handler.get());

    // Initialize semantic search engine
    auto search_engine = create_semantic_search_engine(
        embeddings_client, doc_processor, config, logger.get(), error_handler.get());

    if (!search_engine) {
        std::cout << "❌ Failed to initialize semantic search engine\n";
        return;
    }

    // Extract texts for benchmarking
    std::vector<std::string> sample_texts;
    for (const auto& [_, text] : SAMPLE_DOCUMENTS) {
        sample_texts.push_back(text);
    }

    // Run demonstrations
    benchmark_embeddings(embeddings_client, sample_texts);
    demonstrate_document_processing(doc_processor);
    demonstrate_similarity_calculations(embeddings_client);
    demonstrate_semantic_search(embeddings_client, doc_processor, search_engine);

    std::cout << "\n🎯 Embeddings Integration Demo Complete!\n";
    std::cout << "==========================================\n";
    std::cout << "Key Achievements:\n";
    std::cout << "✅ FastEmbed integration (cost-effective alternative to OpenAI)\n";
    std::cout << "✅ Document chunking with multiple strategies\n";
    std::cout << "✅ Semantic search with similarity scoring\n";
    std::cout << "✅ Batch processing for performance\n";
    std::cout << "✅ CPU-based inference (no GPU required)\n";
    std::cout << "✅ Regulatory document analysis capabilities\n";
    std::cout << "✅ Configurable embedding models and parameters\n";
}

/**
 * @brief Main function
 */
int main(int argc, char* argv[]) {
    try {
        demonstrate_embeddings();
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "❌ Demo failed with exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
